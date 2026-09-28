// Real programs. c4, asciifluid and dhrystone are linked by ld on Braam from
// the objects and SDK archives their build linked, with the flags their
// link.txt passes, and each program's own tests run against the result. Then
// ld links itself; that ld links c4 to the same bytes, and links itself to
// the same bytes again.

import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { APPS, boot, die, get, manifest, ok, plant, run, sections } from "./wasmlib.mjs";

const PROGRAMS = {
    c4: {
        dir: "build/devel/c4",
        target: "bin_c4",
        tests: ["devel/c4/test/run.mjs", "devel/c4/test/selfhost.mjs",
                "devel/c4/test/interrupt.mjs"],
    },
    asciifluid: {
        dir: "build/games/asciifluid",
        target: "bin_asciifluid",
        tests: ["games/asciifluid/test/frames.mjs", "games/asciifluid/test/colour.mjs",
                "games/asciifluid/test/interrupt.mjs"],
    },
    dhrystone: {
        dir: "build/benchmarks/dhrystone",
        target: "bin_dhrystone",
        tests: ["benchmarks/dhrystone/test/interrupt.mjs"],
    },
    ld: { dir: "build/devel/wasm/ld", target: "bin_ld", tests: [] },
};

const m = manifest();
const H = await boot();
const tmp = mkdtempSync(join(tmpdir(), "ld-relink-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
plant(H, "/bin/ld", new Uint8Array(readFileSync(m.ld)));

const bad = [];

// The link line CMake wrote: -Wl, flags, objects and archives, as ld's
// arguments. Every input is planted in /tmp, objects prefixed with <name>.
function prepare(name) {
    const p = PROGRAMS[name];
    const dir = join(APPS, p.dir);
    const words = readFileSync(join(dir, "CMakeFiles", `${p.target}.dir`, "link.txt"), "utf8")
        .trim().split(/\s+/);
    const args = [];
    for (let i = 1; i < words.length; i++) {
        const w = words[i];
        if (w === "-o") {
            i++;
        } else if (w.startsWith("-Wl,")) {
            args.push(...w.slice(4).split(","));
        } else if (w.endsWith(".obj") || w.endsWith(".a")) {
            const at = `/tmp/${w.endsWith(".a") ? "" : name + "."}${basename(w)}`;
            if (!H.store.files.has(at))
                plant(H, at, new Uint8Array(readFileSync(join(dir, w))));
            args.push(at);
        }
    }
    plant(H, `/tmp/${name}.rsp`, [...args, "-o", `/tmp/${name}.out`].join("\n") + "\n");
    return join(dir, `${name}.wasm`);
}

// Links `name` with the linker at /bin/<linker>; the bytes, or null.
function link(name, linker = "ld") {
    for (const f of [`/tmp/${name}.out`, "/tmp/e", "/tmp/s"])
        H.store.files.delete(f);
    run(H, `cd /tmp; ${linker} @${name}.rsp 2>e; echo $? >s`);
    const status = get(H, "/tmp/s"), err = get(H, "/tmp/e");
    if (status !== "0\n" || err) {
        bad.push(`${name}: ${linker} fails with ${JSON.stringify(status)}: ${err}`);
        return null;
    }
    return Buffer.from(H.store.files.get(`/tmp/${name}.out`));
}

function stamp(bytes) {
    return Buffer.from(sections(bytes).find((s) => s.name === "braam").body).toString("hex");
}

const sizes = [];
const outputs = {};
for (const name of Object.keys(PROGRAMS)) {
    const reference = new Uint8Array(readFileSync(prepare(name)));
    const bytes = link(name);
    if (!bytes)
        continue;
    outputs[name] = bytes;
    if (!WebAssembly.validate(bytes)) {
        bad.push(`${name}: not valid wasm`);
        continue;
    }
    // The stamp is stamp.py's, less nothing.
    if (stamp(bytes) !== stamp(reference))
        bad.push(`${name}: braam section ${stamp(bytes)}, stamp.py wrote ${stamp(reference)}`);
    sizes.push(`${name} ${bytes.length} (wasm-ld ${reference.length})`);
    const file = join(tmp, `${name}.wasm`);
    writeFileSync(file, bytes);
    for (const t of PROGRAMS[name].tests) {
        const r = spawnSync(process.execPath, [join(APPS, t), `--binary=${file}`],
                            { encoding: "utf8" });
        if (r.status !== 0)
            bad.push(`${name}: ${t} fails:\n${r.stdout}${r.stderr}`);
    }
}

// Self-hosting: the ld that ld linked links c4 as ld did, and itself as it
// was linked.
if (outputs.ld && outputs.c4) {
    plant(H, "/bin/ld2", outputs.ld);
    const c4 = link("c4", "ld2");
    if (c4 && !c4.equals(outputs.c4))
        bad.push("c4: the self-linked ld links it differently");
    const ld = link("ld", "ld2");
    if (ld && !ld.equals(outputs.ld))
        bad.push("ld: the self-linked ld links itself differently");
}

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`relinked on Braam and passed their tests: ${sizes.join(", ")}; ld links itself to a fixed point`);
