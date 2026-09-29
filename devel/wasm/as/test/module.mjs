// as --module against wabt. Every module of the 233 scripts wast2json reads
// is assembled, and its bytes must be those wast2json wrote for it. The
// other 25 use GC's text syntax, which wabt does not have: there V8 must
// load each module, and disasm must read them back. Invalid modules `as`
// refuses, which spec.mjs checks.
// Their encodings are held to bytes read against the binary format by
// hand, in GOLDEN.
//
// wast2json has every feature --enable-all gives it but compact imports, an
// encoding of the import section outside the language. Where wast2json
// writes no binary, an invalid module it keeps as text, there is nothing to
// compare; and where wabt is wrong, as differs, as KNOWN lists.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";
import { modules } from "./wast.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const BUILD = join(HERE, "../../../../build/devel/wasm");
const AS = join(BUILD, "as/as.wasm");
const DISASM = join(BUILD, "disasm/disasm.wasm");
const SUITE = join(HERE, "suite/core");
const FEATURES = ["--enable-threads", "--enable-function-references", "--enable-code-metadata",
                  "--enable-gc", "--enable-custom-page-sizes", "--enable-wide-arithmetic"];

// Script:line, and why.
const KNOWN = {
    "align.wast:1016": "align=2**63: wabt truncates it to 32 bits, and writes 2**0",
};

function die(msg) {
    console.error("module: " + msg);
    process.exit(1);
}

for (const f of [AS, DISASM])
    if (!existsSync(f))
        die(`no ${f} — run make`);
const version = spawnSync("wast2json", ["--version"], { encoding: "utf8" });
if (version.error || version.stdout.trim() !== "1.0.42")
    die(`wants wast2json 1.0.42, found ${version.error ? "none" : version.stdout.trim()}`);

const as = await assembler(AS, { disasm: DISASM });
const tmp = mkdtempSync(join(tmpdir(), "as-module-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const bad = [];
const hex = (b, at) => [...b.subarray(Math.max(0, at - 4), at + 8)]
    .map((x) => x.toString(16).padStart(2, "0")).join(" ");

// wabt's modules of a script, in order: { line, bytes }, bytes "text"
// where it wrote none; null when it cannot read the script.
function wabt(script, n) {
    const json = join(tmp, `s${n}.json`);
    const r = spawnSync("wast2json", [...FEATURES, join(SUITE, script), "-o", json]);
    if (r.status !== 0)
        return null;
    return JSON.parse(readFileSync(json, "utf8")).commands.filter((c) => c.filename).map((c) => ({
        line: c.line,
        bytes: c.module_type === "text" ? "text" : readFileSync(join(tmp, c.filename)),
    }));
}

// ------------------------------------------------------------ golden bytes

// [what, source, the bytes after the header]: each checked by hand, and
// each must load in V8.
const T64 = "(type (func)) ".repeat(64);
const GOLDEN = [
    ["rec group, sub, packed fields", `(rec (type $a (struct (field (ref $b)))) (type $b (sub (array (mut i8)))))`,
     "01 0d 01 4e 02 5f 01 64 01 00 50 00 5e 78 01"],
    ["sub final with a super", `(type $a (sub (struct (field i16 (mut i32)))))
(type (sub final $a (struct (field i16 (mut i32) f64))))`,
     "01 14 02 50 00 5f 02 77 00 7f 01 4f 01 00 5f 03 77 00 7f 01 7c 00"],
    ["empty rec", `(rec)`, "01 03 01 4e 00"],
    ["reference types", `(type $t (func))
(func (param (ref null $t) (ref $t) (ref null func) (ref any)) (result i31ref) unreachable)`,
     "01 0f 02 60 00 00 60 04 63 00 64 00 70 64 6e 01 6c 03 02 01 01 0a 05 01 03 00 00 0b"],
    ["casts", `(type $s (struct (field i32))) (func (param anyref) (result i32)
  (block (result (ref null $s)) local.get 0 br_on_cast 0 anyref (ref null $s) drop unreachable)
  ref.test (ref $s)
  (block (result anyref) local.get 0 br_on_cast_fail 0 anyref (ref $s) ref.cast (ref null $s) drop unreachable)
  drop)`,
     "01 0a 02 5f 01 7f 00 60 01 6e 01 7f 03 02 01 01 0a 26 01 24 00 02 63 00 20 00 fb 18 03 00 6e " +
     "00 1a 00 0b fb 14 00 02 6e 20 00 fb 19 01 00 6e 00 fb 17 00 1a 00 0b 1a 0b"],
    ["array.new_fixed", `(type $a (array i32)) (func (result (ref $a)) i32.const 1 i32.const 2 array.new_fixed $a 2)`,
     "01 09 02 5e 7f 00 60 00 01 64 00 03 02 01 01 0a 0c 01 0a 00 41 01 41 02 fb 08 00 02 0b"],
    ["table and global of a concrete type", `(type $f (func)) (table 1 (ref null $f) (ref.null $f))
(global (ref null $f) (ref.null $f))`,
     "01 04 01 60 00 00 04 0a 01 40 00 63 00 00 01 d0 00 0b 06 07 01 63 00 00 d0 00 0b"],
    ["type index 64, an s33 of two bytes", `${T64}(type $t (struct)) (func (result (ref null $t)) ref.null $t)
(func i32.const 0 i64.const 0 block (param i32 i64) drop drop end)`,
     "01 ce 01 43 " + "60 00 00 ".repeat(64) + "5f 00 60 00 01 63 c0 00 60 02 7f 7e 00 03 03 02 41 00 " +
     "0a 14 02 05 00 d0 c0 00 0b 0c 00 41 00 42 00 02 c2 00 1a 1a 0b 0b"],
];
{
    const inputs = Object.fromEntries(GOLDEN.map(([, src], k) => [`g${k}.s`, src]));
    const r = as.run(["--module", ...Object.keys(inputs)], inputs);
    GOLDEN.forEach(([what, , want], k) => {
        const b = r.files[`g${k}.wasm`];
        if (!b)
            return bad.push(`${what}: not written: ${r.err}`);
        const got = [...b.subarray(8)].map((x) => x.toString(16).padStart(2, "0")).join(" ");
        if (got !== want.trim())
            bad.push(`${what}: ${got}\n    expected ${want.trim()}`);
        try {
            new WebAssembly.Module(b);
        } catch (e) {
            bad.push(`${what}: V8 refuses: ${e.message}`);
        }
    });
}

// ------------------------------------------------------------ the suite

const scripts = readdirSync(SUITE, { recursive: true }).filter((f) => f.endsWith(".wast")).sort();
const cases = [];
let read = 0;
scripts.forEach((f, n) => {
    // Paired in order; wabt's line is 0 for some quoted modules.
    const w = wabt(f, n);
    const ms = modules(readFileSync(join(SUITE, f)));
    read += w !== null;
    if (w && w.length !== ms.length)
        return bad.push(`${f}: ${ms.length} modules, wast2json wrote ${w.length}`);
    ms.forEach((m, k) => {
        if (w && w[k].line && w[k].line !== m.at)
            bad.push(`${f}:${m.line}: wast2json's module ${k} is at line ${w[k].line}`);
        if (m.kind !== "binary" && m.command !== "assert_malformed" && m.command !== "assert_invalid")
            cases.push({ ...m, script: f, wabt: w && w[k].bytes });
    });
});

let same = 0, loaded = 0, other = 0, text = 0, known = 0;
for (let at = 0; at < cases.length; at += 500) {
    const batch = cases.slice(at, at + 500);
    const inputs = Object.fromEntries(batch.map((c, k) => [`m${k}.s`, c.bytes]));
    const r = as.run(["--module", ...Object.keys(inputs)], inputs);
    if (r.err)
        bad.push(`as: ${r.err.slice(0, 300)}`);
    const gc = [];
    batch.forEach((c, k) => {
        const where = `${c.script}:${c.line}`;
        const got = r.files[`m${k}.wasm`];
        if (!got)
            return bad.push(`${where}: not written`);
        if (c.wabt === "text")
            return text++;
        if (c.wabt) {
            const want = new Uint8Array(c.wabt);
            const i = got.findIndex((b, j) => b !== want[j]);
            if (KNOWN[where]) {
                if (i < 0 && got.length === want.length)
                    bad.push(`${where}: equal to wabt's, but listed as known to differ`);
                known++;
            } else if (i >= 0 || got.length !== want.length) {
                const d = i >= 0 ? i : Math.min(got.length, want.length);
                bad.push(`${where}: at ${d}: ${hex(got, d)}, wabt ${hex(want, d)}`);
            } else {
                same++;
            }
            return;
        }
        other++;
        gc.push(k);
        if (c.command === "module") {
            try {
                new WebAssembly.Module(got);
                loaded++;
            } catch (e) {
                bad.push(`${where}: V8 refuses: ${e.message}`);
            }
        }
    });
    if (gc.length) {
        const files = Object.fromEntries(gc.map((k) => [`m${k}.wasm`, r.files[`m${k}.wasm`]]));
        const d = as.run(Object.keys(files), files, null, "disasm");
        for (const l of d.err.split("\n").filter((l) => l)) {
            const m = /^disasm: error: m(\d+)\.wasm: (.*)$/.exec(l);
            const c = m && batch[Number(m[1])];
            bad.push(c ? `${c.script}:${c.line}: disasm: ${m[2]}` : l);
        }
    }
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 60).join("\n  "));
console.log(`module ok: ${GOLDEN.length} golden; ${same} modules of ${read} scripts equal wabt's, ${known} differ where ` +
            `wabt is wrong, ${text} have no binary; ${other} of ${scripts.length - read} more ` +
            `read back, ${loaded} loaded by V8`);
