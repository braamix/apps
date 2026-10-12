// The passes on Braam against the host's, over every C source of this tree:
// the compiler's own, its C library's, and the test programs. vcpp makes the
// host's .i of each; then vparse, vlower and vgenwasm, from the host's .i, make
// the host's .s, or refuse what the host refuses. The .ast and .tac between
// them are word streams of the build's own size_t, so they are not compared.

import { execFileSync, spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { APPS, TREE, fixture } from "./ccfix.mjs";

const { die, boot } = fixture("corpus");
const HOST = join(APPS, "build/host-cc/stage/bin");
const PASS_DIRS = ["libutil", "ast", "scanner", "parser", "tac", "semantic", "translator",
                   "optimize", "backend/common", "backend/wasm", "cpp"];

// Each source's target and include path, as it is built.
function how(src) {
    let target = "wasm32-braam", inc = [];
    if (/^(test\/programs\/braam_tests|libc\/wasm32\/braam|docs\/examples)\//.test(src))
        inc = ["-Ilibc/wasm32/braam/include"];
    else if (/^(libc|test)\//.test(src))
        target = "wasm32";
    else
        inc = ["-D_CORO_NO_DEFER_YIELD", "-Ilibc/wasm32/braam/include"];
    inc.push("-Ilibc/wasm32/include", "-Ilibc/ilp32/include", "-Ilibc/common/include");
    if (!src.startsWith("libc/"))
        inc.push(...PASS_DIRS.map((d) => "-I" + d));
    return { target, args: ["-t", target, "-nostdinc", ...inc] };
}

const files = execFileSync("git", ["ls-files", "--cached", "--others", "--exclude-standard"],
                           { cwd: TREE, encoding: "utf8" }).split("\n");
const sources = files.filter((f) => f.endsWith(".c"));

// The host's .i and .s.
const host = mkdtempSync(join(tmpdir(), "vcc-corpus-"));
const ref = [];
for (const [k, src] of sources.entries()) {
    const { target, args } = how(src);
    const i = join(host, `${k}.i`);
    if (spawnSync(join(HOST, "vcpp"), [...args, src, i], { cwd: TREE }).status !== 0)
        continue; // cpp refuses it: an #error, or a header of another target
    let s = null;
    const ast = join(host, `${k}.ast`), tac = join(host, `${k}.tac`);
    if (spawnSync(join(HOST, "vparse"), [i, ast]).status === 0 &&
        spawnSync(join(HOST, "vlower"), ["-t", target, ast, tac]).status === 0 &&
        spawnSync(join(HOST, "vgenwasm"), [tac, join(host, `${k}.s`)]).status === 0)
        s = readFileSync(join(host, `${k}.s`));
    ref.push({ k, src, target, args, i: readFileSync(i), s });
}
rmSync(host, { recursive: true });

const { H, put, sh, store } = await boot();
for (const f of files)
    if (/\.[ch]$/.test(f))
        put("/tmp/cc/" + f, new Uint8Array(readFileSync(join(TREE, f))));
for (const { k, i } of ref)
    put(`/tmp/h/${k}.i`, i);

let script = `PATH=$PATH:${store}/libexec\nmkdir /tmp/out\ncd /tmp/cc\n`;
for (const { k, src, args } of ref)
    script += `vcpp ${args.join(" ")} ${src} /tmp/out/${k}.i 2>/dev/null\n`;
script += "cd /tmp/out\n";
for (const { k, target } of ref)
    script += `vparse ../h/${k}.i ${k}.ast 2>/dev/null && ` +
              `vlower -t ${target} ${k}.ast ${k}.tac 2>/dev/null && ` +
              `vgenwasm ${k}.tac ${k}.s 2>/dev/null\n`;
const r = sh(script);
if (r.err)
    die(`the script: ${r.err}`);

const bad = [];
const same = (a, b) => a && b && Buffer.compare(Buffer.from(a), b) === 0;
let compiled = 0;
for (const { k, src, i, s } of ref) {
    const got = (e) => H.store.files.get(`/tmp/out/${k}.${e}`);
    if (!same(got("i"), i))
        bad.push(`${src}: vcpp's .i is not the host's`);
    if (s) {
        compiled++;
        if (!same(got("s"), s))
            bad.push(`${src}: ${got("s") ? "the .s is not the host's" : "refused on Braam"}`);
    } else if (got("s")?.length) {
        bad.push(`${src}: compiled on Braam, refused on the host`);
    }
}
if (bad.length)
    die(`${bad.length} differ:\n` + bad.join("\n"));
console.log(`corpus ok: ${ref.length} sources preprocessed as the host does, ` +
            `${compiled} compiled to the host's .s and ${ref.length - compiled} refused by both`);
