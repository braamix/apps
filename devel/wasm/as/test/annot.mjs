// Annotations, as README.md describes them. The suite once more with
// --debug-names, against wast2json's names. The spec's test/custom
// scripts: their modules
// held to the layout their sections must have, since wabt has no @name and
// puts every @custom last, and their assertions to the reference's
// messages. Branch hints, @custom and names against wat2wasm, module and
// object, where it writes the same. And the data annotations: bytes that
// run in V8, and an object llvm-objdump reads as clang's.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { manifest } from "../../ld/test/wasmlib.mjs";
import { assembler } from "../host.mjs";
import { modules } from "./wast.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");
const CORE = join(HERE, "suite/core");
const CUSTOM = join(HERE, "suite/custom");
const FEATURES = ["--enable-threads", "--enable-function-references", "--enable-code-metadata",
                  "--enable-gc", "--enable-custom-page-sizes", "--enable-wide-arithmetic"];
const OBJDUMP = manifest().objdump;

// Where wabt is wrong, as module.mjs lists it.
const KNOWN = { "align.wast:1016": true };

function die(msg) {
    console.error("annot: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
const version = spawnSync("wast2json", ["--version"], { encoding: "utf8" });
if (version.error || version.stdout.trim() !== "1.0.42")
    die(`wants wast2json 1.0.42, found ${version.error ? "none" : version.stdout.trim()}`);

const as = await assembler(AS);
const tmp = mkdtempSync(join(tmpdir(), "as-annot-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const bad = [];
const hex = (b) => [...b].map((x) => x.toString(16).padStart(2, "0")).join(" ");

// ------------------------------------------------------------ bytes

const utf8 = (s) => [...new TextEncoder().encode(s)];
const uleb = (n) => {
    const b = [];
    do {
        b.push((n & 0x7f) | (n >= 0x80 ? 0x80 : 0));
        n >>>= 7;
    } while (n);
    return b;
};
const name = (s) => [...uleb(utf8(s).length), ...utf8(s)];
const section = (id, body) => [id, ...uleb(body.length), ...body];
const custom = (n, body) => section(0, [...name(n), ...(typeof body === "string" ? utf8(body) : body)]);
const bytes = (h) => h.split(/\s+/).filter((x) => x).map((x) => parseInt(x, 16));

// A module's sections: { id, name, body, at }, body without the name.
function sections(b) {
    const out = [];
    const leb = (at) => {
        let v = 0, s = 0, x;
        do {
            x = b[at.i++];
            v += (x & 0x7f) * 2 ** s;
            s += 7;
        } while (x & 0x80);
        return v;
    };
    const at = { i: 8 };
    while (at.i < b.length) {
        const id = b[at.i++], n = leb(at), end = at.i + n;
        let nm = null;
        if (id === 0) {
            const k = leb(at);
            nm = new TextDecoder().decode(b.subarray(at.i, at.i + k));
            at.i += k;
        }
        out.push({ id, name: nm, body: b.subarray(at.i, end), at: at.i });
        at.i = end;
    }
    return out;
}

// wabt's object with clang's reloc section names: reloc.CODE, and
// reloc.<name> for a custom section, where wabt writes reloc.Code and
// reloc.Custom.
function clangNames(b) {
    const secs = sections(b);
    const out = [...b.subarray(0, 8)];
    for (const s of secs) {
        if (s.id === 0 && s.name.startsWith("reloc.")) {
            const target = secs[s.body[0]]; // one-byte section index
            const n = "reloc." + (target.id ? s.name.slice(6).toUpperCase() : target.name);
            out.push(...custom(n, [...s.body]));
        } else if (s.id === 0) {
            out.push(...custom(s.name, [...s.body]));
        } else {
            out.push(...section(s.id, [...s.body]));
        }
    }
    return new Uint8Array(out);
}

// ------------------------------------------------------------ the suite, named

// wabt's modules of a script with --debug-names, in order; null when it
// cannot read the script.
function wabt(script, n) {
    const json = join(tmp, `s${n}.json`);
    const r = spawnSync("wast2json", [...FEATURES, "--debug-names", join(CORE, script), "-o", json]);
    if (r.status !== 0)
        return null;
    return JSON.parse(readFileSync(json, "utf8")).commands.filter((c) => c.filename).map((c) =>
        c.module_type === "text" ? "text" : readFileSync(join(tmp, c.filename)));
}

let same = 0, read = 0;
{
    const cases = [];
    readdirSync(CORE, { recursive: true }).filter((f) => f.endsWith(".wast")).sort().forEach((f, n) => {
        const w = wabt(f, n);
        if (!w)
            return;
        read++;
        modules(readFileSync(join(CORE, f))).forEach((m, k) => {
            if (m.kind !== "binary" && m.command !== "assert_malformed" &&
                m.command !== "assert_invalid" && w[k] !== "text" &&
                !KNOWN[`${f}:${m.line}`])
                cases.push({ ...m, script: f, wabt: w[k] });
        });
    });
    for (let at = 0; at < cases.length; at += 500) {
        const batch = cases.slice(at, at + 500);
        const inputs = Object.fromEntries(batch.map((c, k) => [`m${k}.s`, c.bytes]));
        const r = as.run(["--module", "--debug-names", ...Object.keys(inputs)], inputs);
        batch.forEach((c, k) => {
            const got = r.files[`m${k}.wasm`];
            const want = new Uint8Array(c.wabt);
            if (!got)
                return bad.push(`${c.script}:${c.line}: not written: ${r.err.slice(0, 200)}`);
            const i = got.findIndex((b, j) => b !== want[j]);
            if (i >= 0 || got.length !== want.length)
                bad.push(`${c.script}:${c.line}: --debug-names differs at ${i >= 0 ? i : got.length}`);
            else
                same++;
        });
    }
}

// ------------------------------------------------------------ test/custom

const TYPE = section(1, bytes("01 60 00 00"));
const FUNC = section(3, bytes("01 00"));

// Script:line, and the bytes after the header; or a check of its own.
const LAYOUT = {
    "custom/custom_annot.wast:1": [
        ...TYPE, ...FUNC,
        ...custom("my-section2", "more-contents-bytes2"), // after func
        ...custom("my-section2", "more-contents-bytes3"),
        ...custom("my-section2", "more-contents-bytes1"), // before global
        ...custom("my-section2", "more-contents-bytes4"),
        ...section(6, bytes("01 7f 00 41 00 0b")),
        ...section(10, bytes("01 02 00 0b")),
        ...custom("my-section1", "contents-bytes1"), // after last, in order
        ...custom("my-section2", "more-contents-bytes0"),
        ...custom("my-section1", "contents-bytes2"),
        ...custom("my-section2", "more-contents-bytes5"),
        ...custom("my-section3", ""),
        ...custom("my-section4", "123"),
        ...custom("", ""),
    ],
    "custom/custom_annot.wast:19": custom("bla", ""),
    "custom/custom_annot.wast:20": custom("bla", ""),
    "name/name_annot.wast:3": custom("name", [0, ...uleb(name("Modül").length), ...name("Modül")]),
    "name/name_annot.wast:5": custom("name", [0, ...uleb(name("Modül").length), ...name("Modül")]),
    "name/name_annot.wast:25": [
        ...TYPE, ...section(3, bytes("02 00 00")), ...section(10, bytes("02 02 00 0b 02 00 0b")),
        ...custom("name", [1, ...uleb(1 + 2 * (1 + name("λ").length)), 2, 0, ...name("λ"), 1, ...name("λ")]),
    ],
    "name/name_annot.wast:34": [
        ...TYPE, ...section(13, bytes("02 00 00 00 00")),
        ...custom("name", [11, ...uleb(1 + 2 * (1 + name("θ").length)), 2, 0, ...name("θ"), 1, ...name("θ")]),
    ],
    "metadata.code.branch_hint/branch_hint.wast:1": hinted,
};

// branch_hint.wast's first module: its hints, a function index and the
// offsets and hints in its body; each on an if, right before the code.
function hinted(b) {
    const want = [[1, [[8, 0]]], [2, [[8, 1]]], [3, [[3, 0], [30, 1], [56, 0]]]];
    const secs = sections(b);
    const k = secs.findIndex((s) => s.name === "metadata.code.branch_hint");
    if (k < 0 || secs[k + 1]?.id !== 10)
        return "no metadata.code.branch_hint right before the code";
    const m = secs[k].body, code = secs[k + 1].body;
    const got = [];
    for (let i = 1, f = 0; f < m[0]; f++) {
        const fn = m[i++], n = m[i++], hs = [];
        for (let h = 0; h < n; h++, i += 3)
            hs.push([m[i], m[i + 2]]);
        got.push([fn, hs]);
    }
    if (JSON.stringify(got) !== JSON.stringify(want))
        return `hints ${JSON.stringify(got)}, not ${JSON.stringify(want)}`;
    // Bodies: count, then size and body each; one-byte sizes here.
    const bodies = [];
    for (let i = 1; bodies.length < code[0]; i += 1 + code[i])
        bodies.push(code.subarray(i + 1, i + 1 + code[i]));
    for (const [fn, hs] of want)
        for (const [at] of hs)
            if (bodies[fn][at] !== 0x04)
                return `function ${fn}: offset ${at} is not an if`;
    return null;
}

let customs = 0, refused = 0;
for (const f of readdirSync(CUSTOM, { recursive: true }).filter((f) => f.endsWith(".wast")).sort()) {
    const ms = modules(readFileSync(join(CUSTOM, f)));
    const inputs = Object.fromEntries(ms.map((m, k) => [`c${k}.s`, m.bytes]));
    const r = as.run(["--module", ...Object.keys(inputs)], inputs);
    const said = new Map();
    for (const l of r.err.split("\n").filter((l) => l))
        said.set(l.slice(0, l.indexOf(":")), l.replace(/^[^:]*:\d+:\d+: error: /, ""));
    ms.forEach((m, k) => {
        const where = `${f}:${m.line}`;
        const got = r.files[`c${k}.wasm`];
        if (m.command === "module") {
            if (!got)
                return bad.push(`${where}: refused: ${said.get(`c${k}.s`)}`);
            const want = LAYOUT[where];
            if (typeof want === "function") {
                const why = want(got);
                if (why)
                    bad.push(`${where}: ${why}`);
            } else if (!want) {
                bad.push(`${where}: no layout to hold it to`);
            } else if (hex(got.subarray(8)) !== hex(want)) {
                bad.push(`${where}: ${hex(got.subarray(8))}\n    expected ${hex(want)}`);
            }
            return customs++;
        }
        const msg = said.get(`c${k}.s`);
        if (got || !msg?.startsWith(m.message))
            return bad.push(`${where}: ${m.command}: ${msg ?? "accepted"}, expected ${m.message}`);
        refused++;
    });
}

// ------------------------------------------------------------ against wat2wasm

// Each written by as and by wat2wasm, module and object, and equal but for
// clang's reloc section names. Not a hint on a folded instruction, which
// wabt puts on the first of its operands; nor, in an object, an elem
// segment, whose function indices as relocates and wabt does not.
const WABT = [
    ["flat branch hints", `(module (func $g) (func $f (param i32)
  call $g local.get 0 (@metadata.code.branch_hint "\\01") if call $g end
  local.get 0 (@metadata.code.branch_hint "\\00") br_if 0)
 (func $h (param i32) local.get 0 (@metadata.code.branch_hint "\\00") br_if 0
  local.get 0 (@metadata.code.branch_hint "\\01") (if (then))))`],
    ["@custom, in no place", `(module (@custom "a" "x") (func) (@custom "b" "y" "z")
 (data "d") (@custom "a" ""))`],
    ["names from ids", `(module $m (type $t (func (param i32))) (import "a" "b" (func $imp (param i32)))
 (func $f (type $t) (param $p i32) (local $l i64) (local i32) (local $"z z" f32))
 (func) (table $tab 1 funcref) (memory $mem 1) (global $g i32 (i32.const 0))
 (data $d "x") (elem $e func $f))`, "module"],
    ["names and hints", `(module (func $a (param $x i32) local.get $x
 (@metadata.code.branch_hint "\\00") if end) (func $b (param i32)) (global $g (mut i32) (i32.const 1)))`],
];
for (const [what, src, only] of WABT) {
    writeFileSync(join(tmp, "w.wat"), src);
    for (const object of only ? [false] : [false, true]) {
        const args = [...FEATURES, "--debug-names", join(tmp, "w.wat"), "-o", join(tmp, "w.out")];
        const w = spawnSync("wat2wasm", object ? ["-r", ...args] : args, { encoding: "utf8" });
        if (w.status !== 0) {
            bad.push(`${what}: wat2wasm: ${w.stderr}`);
            continue;
        }
        const want = object ? clangNames(readFileSync(join(tmp, "w.out"))) : readFileSync(join(tmp, "w.out"));
        const r = as.run([...(object ? [] : ["--module"]), "--debug-names", "w.s"], { "w.s": src });
        const got = r.files[object ? "w.o" : "w.wasm"];
        if (!got)
            bad.push(`${what}: ${r.err}`);
        else if (hex(got) !== hex(want))
            bad.push(`${what}${object ? ", object" : ""}: ${hex(got)}\n    wabt ${hex(want)}`);
    }
}

// ------------------------------------------------------------ data annotations

// The C of test/link/data.c, by hand: what clang makes of
//   int counter = 5; const char msg[] = "hi"; int *ptr = &counter;
//   int buf[2]; int get(void) { return counter + buf[1] + *ptr + msg[1]; }
const DATA = `(module (memory 1)
  (data $counter (@sym align=4) "\\05\\00\\00\\00")
  (data $msg (@sym rodata) "hi\\00")
  (data $ptr (@sym align=4) (@reloc $counter))
  (data $buf (@sym bss align=16) "\\00\\00\\00\\00\\00\\00\\00\\00")
  (func $get (export "get") (result i32)
    i32.const (@reloc $counter) i32.load
    i32.const 0 i32.load (@reloc $buf 4) i32.add
    i32.const (@reloc $ptr) i32.load i32.load i32.add
    i32.const (@reloc $msg 1) i32.load8_u i32.add))`;
{
    const r = as.run(["--module", "d.s"], { "d.s": DATA });
    const m = r.files["d.wasm"];
    const want = bytes(`01 05 01 60 00 01 7f 03 02 01 00 05 03 01 00 01 07 07 01 03 67 65 74 00 00
        0a 1e 01 1c 00 41 00 28 02 00 41 00 28 02 14 6a 41 08 28 02 00 28 02 00 6a 41 05 2d 00 00
        6a 0b 0b 28 04 00 41 00 0b 04 05 00 00 00 00 41 04 0b 03 68 69 00 00 41 08 0b 04 00 00 00
        00 00 41 10 0b 08 00 00 00 00 00 00 00 00`);
    if (!m)
        bad.push(`data module: ${r.err}`);
    else if (hex(m.subarray(8)) !== hex(want))
        bad.push(`data module: ${hex(m.subarray(8))}\n    expected ${hex(want)}`);
    else if (new WebAssembly.Instance(new WebAssembly.Module(m)).exports.get() !== 5 + 0 + 5 + 105)
        bad.push("data module: get() is wrong");
}
{
    const r = as.run(["d.s"], { "d.s": DATA });
    const o = r.files["d.o"];
    if (!o) {
        bad.push(`data object: ${r.err}`);
    } else {
        writeFileSync(join(tmp, "d.o"), o);
        const d = spawnSync(OBJDUMP, ["-t", "-r", "-h", join(tmp, "d.o")], { encoding: "utf8" });
        const got = d.stdout.split("\n").filter((l) => /^(0000|RELOC|\s+\d+ (IMPORT|DATA|CODE))/.test(l))
            .map((l) => l.trim().replace(/\s+/g, " "));
        const want = [
            "1 IMPORT 00000018 00000000",
            "4 CODE 0000002e 00000000 TEXT",
            "5 DATA 00000028 00000000 DATA",
            "00000001 g F CODE 0000002d .hidden get",
            "00000000 g O DATA 00000004 counter",
            "00000004 g O DATA 00000003 msg",
            "00000008 g O DATA 00000004 ptr",
            "00000010 g O DATA 00000008 buf",
            "RELOCATION RECORDS FOR [CODE]:",
            "00000004 R_WASM_MEMORY_ADDR_SLEB counter+0",
            "00000010 R_WASM_MEMORY_ADDR_LEB buf+4",
            "00000017 R_WASM_MEMORY_ADDR_SLEB ptr+0",
            "00000024 R_WASM_MEMORY_ADDR_SLEB msg+1",
            "RELOCATION RECORDS FOR [DATA]:",
            "00000017 R_WASM_MEMORY_ADDR_I32 counter+0",
        ];
        if (d.status !== 0 || got.join("\n") !== want.join("\n"))
            bad.push(`data object: llvm-objdump says\n    ${got.join("\n    ")}${d.stderr}`);
        const secs = sections(o);
        const imp = secs.find((s) => s.id === 2);
        if (!imp || hex(imp.body) !== hex([1, ...name("env"), ...name("__linear_memory"), 2, 0, 1]))
            bad.push("data object: memory 0 is not env.__linear_memory");
        const link = secs.find((s) => s.name === "linking").body;
        const info = [5, ...uleb(1 + 4 * 2 + [".data.counter", ".rodata.msg", ".data.ptr", ".bss.buf"]
            .reduce((n, s) => n + name(s).length, 0)), 4,
                      ...name(".data.counter"), 2, 0, ...name(".rodata.msg"), 0, 0,
                      ...name(".data.ptr"), 2, 0, ...name(".bss.buf"), 4, 0];
        if (!hex(link).endsWith(hex(info)))
            bad.push(`data object: segment info is not ${hex(info)}`);
    }
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 60).join("\n  "));
console.log(`annot ok: ${same} modules of ${read} scripts named as wabt names them; ` +
            `${customs} of test/custom laid out, ${refused} refused with the reference's message; ` +
            `${WABT.length} against wat2wasm; data as clang lays it out`);
