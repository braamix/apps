// as's objects. Every module of the 233 scripts wast2json reads is
// assembled as an object, and its bytes must be those `wast2json -r` wrote
// for it, but for the relocation sections' names, which as spells as clang
// does: reloc.CODE, not reloc.Code. Where decision 1 of Plan.md goes beyond
// wabt, tags and element segments, the bytes differ and are counted. Then
// every valid module's object is read by llvm-nm, llvm-objdump -r,
// wasm-objdump -x, and our nm and disasm -r: nm must print what llvm-nm
// prints, and disasm the relocations llvm-objdump lists for the code.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { manifest } from "../../ld/test/wasmlib.mjs";
import { assembler } from "../host.mjs";
import { modules } from "./wast.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const BUILD = join(HERE, "../../../../build/devel/wasm");
const AS = join(BUILD, "as/as.wasm");
const TOOLS = { nm: join(BUILD, "nm/nm.wasm"), disasm: join(BUILD, "disasm/disasm.wasm") };
const SUITE = join(HERE, "suite/core");
const FEATURES = ["--enable-threads", "--enable-function-references", "--enable-code-metadata",
                  "--enable-gc", "--enable-custom-page-sizes", "--enable-wide-arithmetic"];
const OBJDUMP = manifest().objdump;
const LLVM_NM = join(dirname(OBJDUMP), "llvm-nm");

function die(msg) {
    console.error("object: " + msg);
    process.exit(1);
}

for (const f of [AS, ...Object.values(TOOLS)])
    if (!existsSync(f))
        die(`no ${f} — run make`);
const version = spawnSync("wast2json", ["--version"], { encoding: "utf8" });
if (version.error || version.stdout.trim() !== "1.0.42")
    die(`wants wast2json 1.0.42, found ${version.error ? "none" : version.stdout.trim()}`);

const as = await assembler(AS, TOOLS);
const tmp = mkdtempSync(join(tmpdir(), "as-object-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const bad = [];
const hex = (b, at) => [...b.subarray(Math.max(0, at - 4), at + 8)]
    .map((x) => x.toString(16).padStart(2, "0")).join(" ");

// The sections of a module: [{ id, name, start, end }], `start` its contents'.
function sections(b) {
    const out = [];
    let i = 8;
    const uleb = () => {
        let v = 0, s = 0, c;
        do {
            c = b[i++];
            v += (c & 0x7f) * 2 ** s;
            s += 7;
        } while (c & 0x80);
        return v;
    };
    while (i < b.length) {
        const id = b[i++], size = uleb(), start = i;
        let name = null;
        if (id === 0) {
            const n = uleb();
            name = new TextDecoder().decode(b.subarray(i, i + n));
        }
        out.push({ id, name, start, end: start + size });
        i = start + size;
    }
    return out;
}

// Whether a module imports a tag.
function tagImport(b) {
    const s = sections(b).find((s) => s.id === 2);
    if (!s)
        return false;
    let i = s.start;
    const uleb = () => {
        let v = 0, k = 0, c;
        do {
            c = b[i++];
            v += (c & 0x7f) * 2 ** k;
            k += 7;
        } while (c & 0x80);
        return v;
    };
    const limits = (memory) => {
        const flags = uleb();
        uleb();
        if (flags & 1)
            uleb();
        if (memory && flags & 8)
            uleb();
    };
    const valtype = () => {
        const t = b[i++];
        if (t === 0x63 || t === 0x64)
            uleb();
    };
    for (let n = uleb(); n > 0; n--) {
        for (let k = 0; k < 2; k++) {
            const n = uleb();
            i += n;
        }
        const kind = b[i++];
        if (kind === 4)
            return true;
        if (kind === 0)
            uleb();
        else if (kind === 1)
            valtype(), limits(false);
        else if (kind === 2)
            limits(true);
        else
            valtype(), i++;
    }
    return false;
}

// wabt's object with its relocation sections named as clang names them.
function clangNames(b) {
    const out = new Uint8Array(b);
    for (const s of sections(out))
        if (s.name?.startsWith("reloc.")) {
            let i = s.start;
            while (out[i] & 0x80)
                i++;
            for (let k = i + 1 + "reloc.".length; k < i + 1 + s.name.length; k++)
                out[k] = new TextEncoder().encode(String.fromCharCode(out[k]).toUpperCase())[0];
        }
    return out;
}

// wabt's objects of a script, in order: { line, bytes }; null when it
// cannot write them. wast2json refuses a whole script for one module it
// cannot write, a symbol named twice, so then wat2wasm writes each module,
// and a module it refuses has bytes null.
let refused = 0;
function wabt(script, n, ms) {
    const json = join(tmp, `s${n}.json`);
    const r = spawnSync("wast2json", [...FEATURES, "-r", join(SUITE, script), "-o", json]);
    if (r.status === 0)
        return JSON.parse(readFileSync(json, "utf8")).commands.filter((c) => c.filename).map((c) => ({
            line: c.line,
            bytes: c.module_type === "text" ? "text" : readFileSync(join(tmp, c.filename)),
        }));
    if (!/duplicate symbol/.test(r.stderr))
        return null;
    return ms.map((m) => {
        if (m.kind === "binary" || m.command === "assert_malformed")
            return { line: 0, bytes: null };
        writeFileSync(join(tmp, "w.wat"), m.bytes);
        const w = spawnSync("wat2wasm", [...FEATURES, "-r", "--no-check", "w.wat", "-o", "w.o"], { cwd: tmp });
        refused += w.status !== 0;
        return { line: 0, bytes: w.status === 0 ? readFileSync(join(tmp, "w.o")) : null };
    });
}

const scripts = readdirSync(SUITE, { recursive: true }).filter((f) => f.endsWith(".wast")).sort();
const cases = [];
let read = 0;
scripts.forEach((f, n) => {
    const ms = modules(readFileSync(join(SUITE, f)));
    const w = wabt(f, n, ms);
    read += w !== null;
    if (w && w.length !== ms.length)
        return bad.push(`${f}: ${ms.length} modules, wast2json wrote ${w.length}`);
    ms.forEach((m, k) => {
        if (m.kind !== "binary" && m.command !== "assert_malformed")
            cases.push({ ...m, script: f, wabt: w && (w[k].bytes ?? "refused") });
    });
});

// ------------------------------------------------------------ against wabt

let same = 0, beyond = 0, invalid = 0, text = 0;
const valid = [];
for (let at = 0; at < cases.length; at += 500) {
    const batch = cases.slice(at, at + 500);
    const inputs = Object.fromEntries(batch.map((c, k) => [`m${k}.wat`, c.bytes]));
    const r = as.run(Object.keys(inputs), inputs);
    if (r.err)
        bad.push(`as: ${r.err.slice(0, 300)}`);
    batch.forEach((c, k) => {
        const where = `${c.script}:${c.line}`;
        const got = r.files[`m${k}.o`];
        if (!got)
            return bad.push(`${where}: not written`);
        if (c.command === "module")
            valid.push({ where, bytes: got, wabt: c.wabt !== null });
        if (!c.wabt)
            return;
        if (c.wabt === "refused")
            return;
        if (c.wabt === "text")
            return text++;
        const want = clangNames(c.wabt);
        const i = got.findIndex((b, j) => b !== want[j]);
        if (i < 0 && got.length === want.length)
            return same++;
        // Tags and element segments, where the rule goes beyond wabt's; and
        // invalid modules, which name what is not there, or ref.null a type,
        // whose index wabt relocates as a function's.
        if (sections(got).some((s) => s.id === 13 || s.id === 9) || tagImport(got))
            return beyond++;
        if (c.command === "assert_invalid")
            return invalid++;
        const d = i >= 0 ? i : Math.min(got.length, want.length);
        bad.push(`${where}: at ${d}: ${hex(got, d)}, wabt ${hex(want, d)}`);
    });
}

// ------------------------------------------------------------ read back

// What llvm cannot read, by its message, and why.
const LLVM_LIMITS = [
    [/invalid TableNumber/, "a declarative element segment, whose flags llvm reads as naming a table"],
    [/table section ended prematurely/, "a table with an init expression"],
    [/duplicate symbol name/, "two symbols of one name: wabt's rule names an import by its field"],
    [/invalid opcode in init_expr/, "an init expression with v128.const or a GC instruction"],
    [/bad form/, "GC's sub and rec types"],
    [/Rec group size cannot be 0/, "an empty rec group"],
    [/LEB is outside Varuint1 range|import section ended prematurely/,
     "an imported global of a (ref ...) type, which llvm reads as one byte"],
];

const run = (cmd, args) => spawnSync(cmd, args, { cwd: tmp, encoding: "utf8", maxBuffer: 1 << 30 });

// An llvm tool over many files: its output and its error, by file. A run
// that crashes is run again a file at a time.
function llvm(cmd, args, names) {
    const r = run(cmd, [...args, ...names]);
    if (names.length > 1 && (r.status === null || /PLEASE submit/.test(r.stderr))) {
        const out = new Map(), err = new Map();
        for (const f of names) {
            const one = llvm(cmd, args, [f]);
            out.set(f, one.out.get(f) ?? "");
            if (one.err.has(f))
                err.set(f, one.err.get(f));
        }
        return { out, err };
    }
    const out = new Map(), err = new Map();
    let file = null;
    if (names.length === 1)
        out.set(file = names[0], "");
    for (const l of r.stdout.split("\n")) {
        const m = /^(o\d+\.o):(?:\s+file format.*)?$/.exec(l);
        if (m)
            out.set(file = m[1], "");
        else if (file)
            out.set(file, out.get(file) + l + "\n");
    }
    for (const l of r.stderr.split("\n")) {
        const m = /error: '?(o\d+\.o)'?: (.*)$/.exec(l);
        if (m)
            err.set(m[1], m[2]);
        else if (/LLVM ERROR: (.*)/.test(l) && names.length === 1)
            err.set(names[0], /LLVM ERROR: (.*)/.exec(l)[1]);
    }
    // llvm-objdump stops at a file it refuses: the rest again.
    const rest = names.filter((f) => !out.has(f) && !err.has(f));
    if (rest.length && rest.length < names.length) {
        const more = llvm(cmd, args, rest);
        for (const [f, t] of more.out)
            out.set(f, t);
        for (const [f, e] of more.err)
            err.set(f, e);
    }
    return { out, err };
}

// Our tool's output by file, as llvm's.
function ours(tool, args, files) {
    const r = as.run([...args, ...Object.keys(files)], files, null, tool);
    const out = new Map();
    let file = Object.keys(files).length === 1 ? Object.keys(files)[0] : null;
    for (const l of r.out.split("\n")) {
        const m = /^(o\d+\.o):(?:\s+file format.*)?$/.exec(l);
        if (m)
            file = m[1];
        else if (file)
            out.set(file, (out.get(file) ?? "") + l + "\n");
    }
    const err = r.err.split("\n").filter((l) => l && !/no symbols$/.test(l));
    return { out, err };
}

// "offset type value" of each relocation in the code.
const llvmRelocs = (text) => {
    const out = [];
    let code = false;
    for (const l of text.split("\n")) {
        let m;
        if ((m = /^RELOCATION RECORDS FOR \[(\w+)\]/.exec(l)))
            code = m[1] === "CODE";
        else if (code && (m = /^([0-9a-f]{8}) (R_\w+) /.exec(l)))
            // The value's column is 34, or one past a longer type.
            out.push(`${m[1]} ${m[2]} ${l.slice(9 + Math.max(25, m[2].length + 1))}`);
    }
    return out.join("\n");
};
const disasmRelocs = (text) => text.split("\n").map((l) => /^\t+([0-9a-f]{8}):\s+(R_\w+)\t(.*)$/.exec(l))
    .filter((m) => m).map((m) => `${m[1]} ${m[2]} ${m[3]}`).join("\n");

let agreed = 0, limited = 0;
const limits = new Set();
for (let at = 0; at < valid.length; at += 500) {
    const batch = valid.slice(at, at + 500);
    const files = Object.fromEntries(batch.map((c, k) => [`o${k}.o`, c.bytes]));
    for (const [name, b] of Object.entries(files))
        writeFileSync(join(tmp, name), b);
    const names = Object.keys(files);
    const where = (f) => batch[Number(/\d+/.exec(f)[0])].where;

    const lnm = llvm(LLVM_NM, [], names), lo = llvm(OBJDUMP, ["-r"], names);
    const nm = ours("nm", [], files), d = ours("disasm", ["-r"], files);
    for (const e of [...nm.err, ...d.err])
        bad.push(`ours: ${e}`);
    for (const f of names) {
        const why = lnm.err.get(f) ?? lo.err.get(f);
        if (why) {
            const known = LLVM_LIMITS.find(([re]) => re.test(why));
            if (!known)
                bad.push(`${where(f)}: llvm refuses: ${why}`);
            else
                limited++, limits.add(known[1]);
            continue;
        }
        if ((nm.out.get(f) ?? "") !== (lnm.out.get(f) ?? ""))
            bad.push(`${where(f)}: nm prints ${JSON.stringify(nm.out.get(f))}, llvm-nm ` +
                     JSON.stringify(lnm.out.get(f)));
        else {
            const x = disasmRelocs(d.out.get(f) ?? "").split("\n");
            const y = llvmRelocs(lo.out.get(f) ?? "").split("\n");
            const i = x.findIndex((l, k) => l !== y[k]);
            if (i >= 0 || x.length !== y.length)
                bad.push(`${where(f)}: disasm -r has ${JSON.stringify(x[i])}, llvm-objdump -r ` +
                         JSON.stringify(y[i >= 0 ? i : x.length]));
            else
                agreed++;
        }
    }
    const wabtable = names.filter((f) => batch[Number(/\d+/.exec(f)[0])].wabt);
    if (wabtable.length && run("wasm-objdump", ["-x", ...wabtable]).status !== 0)
        for (const f of wabtable) {
            const r = run("wasm-objdump", ["-x", f]);
            if (r.status !== 0)
                bad.push(`${where(f)}: wasm-objdump -x: ${r.stderr.trim().slice(0, 200)}`);
        }
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 60).join("\n  "));
console.log(`object ok: ${same} objects of ${read} scripts equal wabt's, ${beyond} go beyond its rule, ` +
            `${invalid} invalid differ, ${text} have no binary, ${refused} wabt refuses; ${agreed} read alike by llvm, wabt, ` +
            `nm and disasm, ${limited} beyond llvm (${limits.size} kinds)`);
