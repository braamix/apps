// -Map. Every fixture is linked by both linkers with the memory defined
// rather than imported, so neither needs __wasm_init_memory and the modules
// can be equal: byte for byte, but for ld's braam section, which is last.
// The map files must then be equal but for that section's line: plain, with
// --compress-relocations, and with --strip-all.

import { readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { boot, die, FLAGS, get, linkers, manifest, ok } from "./wasmlib.mjs";

const m = manifest();
const H = await boot();
const { tmp, inputs, wasm_ld, ld } = linkers(H, m);

const bad = [];
const BASE = FLAGS.filter((f) => f !== "--import-memory");

// A module without its trailing braam section.
function unstamped(bytes) {
    const tail = Buffer.from([0, 26, 5, ...Buffer.from("braam")]);
    const b = Buffer.from(bytes);
    const at = b.length - 28;
    return at > 0 && b.subarray(at, at + 8).equals(tail) ? b.subarray(0, at) : null;
}

let lines = 0;
for (const [name, fx] of Object.entries(m.fixtures)) {
    for (const extra of [[], ["--compress-relocations", "--strip-debug"], ["--strip-all"]]) {
        const what = `${name} ${extra.join(" ")}`;
        const args = [...BASE, ...extra, ...inputs(fx), "-Map=map"];
        rmSync(join(tmp, "map"), { force: true });
        const want = wasm_ld(args);
        if (want.status !== 0)
            die(`wasm-ld fails on ${what}: ${want.err}`);
        H.store.files.delete("/tmp/map");
        H.store.files.delete("/tmp/out.wasm");
        const got = ld(["--no-import-memory", ...args, "-o", "out.wasm"]);
        if (got.status !== 0) {
            bad.push(`${what}: ld fails: ${got.err}`);
            continue;
        }
        const ours = unstamped(H.store.files.get("/tmp/out.wasm"));
        const theirs = readFileSync(join(tmp, "out.wasm"));
        if (!ours || !ours.equals(theirs)) {
            let at = 0;
            while (ours && at < ours.length && ours[at] === theirs[at])
                at++;
            bad.push(`${what}: modules differ at 0x${at.toString(16)}`);
        }
        const a = (get(H, "/tmp/map") ?? "").split("\n");
        const b = readFileSync(join(tmp, "map"), "utf8").split("\n");
        if (a.length >= 2 && a[a.length - 2].endsWith("CUSTOM(braam)"))
            a.splice(a.length - 2, 1);
        for (let i = 0; i < Math.max(a.length, b.length); i++)
            if (a[i] !== b[i]) {
                bad.push(`${what}: map line ${i + 1}\n  ld:      ${JSON.stringify(a[i])}\n` +
                         `  wasm-ld: ${JSON.stringify(b[i])}`);
                break;
            }
        lines += b.length - 1;
    }
}

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`${Object.keys(m.fixtures).length} fixtures: modules and maps equal wasm-ld's, ` +
   `plain, packed and stripped (${lines} lines)`);
