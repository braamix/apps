// Section sizes, ld against wasm-ld, for every program in the tree:
// `node devel/wasm/ld/sizes.mjs [<build>]`. Each program is linked twice from
// the objects, archives and flags its link.txt names, once by each linker,
// into a directory of its own; the build is only read. wasm-ld is given
// --no-demangle, since ld never demangles, and "other" includes the 26-byte
// braam section, which ld writes and stamp.py adds to wasm-ld's.

import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, statSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const BUILD = resolve(process.argv[2] ?? join(APPS, "build"));
const LD = join(BUILD, "bootstrap/ld.wasm");
const tmp = mkdtempSync(join(tmpdir(), "ld-sizes-"));
// The same file name for both, which the name section carries.
for (const d of ["wasm-ld", "ld"])
    mkdirSync(join(tmp, d));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

// Every bin_<name>.dir/link.txt under the build, the bootstrap's aside.
function links(dir, out = []) {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
        const p = join(dir, e.name);
        if (!e.isDirectory() || p === join(BUILD, "bootstrap") || p.includes("braam-sdk-"))
            continue;
        if (/^bin_.+\.dir$/.test(e.name) && !e.name.startsWith("bin_fx_")) {
            try {
                statSync(join(p, "link.txt"));
                out.push({ name: e.name.slice(4, -4), cwd: resolve(p, "../.."),
                           txt: join(p, "link.txt") });
            } catch {}
        } else {
            links(p, out);
        }
    }
    return out;
}

function args(txt) {
    const words = readFileSync(txt, "utf8").trim().split(/\s+/);
    const out = [];
    for (let i = 1; i < words.length; i++) {
        const w = words[i];
        if (w === "-o")
            i++;
        else if (w.startsWith("-Wl,"))
            out.push(...w.slice(4).split(","));
        else if (w.endsWith(".obj") || w.endsWith(".a"))
            out.push(w);
    }
    return out;
}

const NAMES = { 1: "TYPE", 2: "IMPORT", 3: "FUNCTION", 4: "TABLE", 5: "MEMORY", 6: "GLOBAL",
                7: "EXPORT", 8: "START", 9: "ELEM", 10: "CODE", 11: "DATA", 12: "DATACOUNT" };

function sizes(file) {
    const b = readFileSync(file);
    const out = { total: b.length };
    let at = 8;
    while (at < b.length) {
        const id = b[at++];
        let n = 0, s = 0, c;
        do {
            c = b[at++];
            n |= (c & 0x7f) << s;
            s += 7;
        } while (c & 0x80);
        out[NAMES[id] ?? "custom"] = (out[NAMES[id] ?? "custom"] ?? 0) + n;
        at += n;
    }
    return out;
}

function run(cmd, argv, cwd) {
    const r = spawnSync(cmd, argv, { cwd, encoding: "utf8" });
    if (r.status !== 0) {
        console.error(`${cmd} ${argv.slice(-3).join(" ")}: ${r.stderr}`);
        process.exit(1);
    }
}

const rows = [["program", "wasm-ld", "ld", "CODE", "DATA", "other"]];
const sum = { a: 0, b: 0 };
for (const p of links(BUILD).sort((x, y) => x.name.localeCompare(y.name))) {
    const a = join(tmp, "wasm-ld", `${p.name}.wasm`), b = join(tmp, "ld", `${p.name}.wasm`);
    const argv = args(p.txt);
    run("wasm-ld", [...argv, "--no-demangle", "-o", a], p.cwd);
    run(process.execPath, [join(APPS, "devel/wasm/ld/host.mjs"), LD, ...argv, "-o", b], p.cwd);
    const x = sizes(a), y = sizes(b);
    const d = (k) => (y[k] ?? 0) - (x[k] ?? 0);
    const other = y.total - x.total - d("CODE") - d("DATA");
    rows.push([p.name, x.total, y.total, d("CODE"), d("DATA"), other]);
    sum.a += x.total;
    sum.b += y.total;
}
rows.push(["all", sum.a, sum.b, "", "", ""]);
const w = rows[0].map((_, i) => Math.max(...rows.map((r) => String(r[i]).length)));
for (const r of rows)
    console.log(r.map((c, i) => i ? String(c).padStart(w[i]) : String(c).padEnd(w[i])).join("  "));
