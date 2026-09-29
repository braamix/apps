// lib/optable.cpp, the instruction table, against three things:
//  * Wasm_Assembly_Language.md §7.5 and §10: every name, nothing more, and
//    the natural alignment N of every memory access;
//  * disasm's table, llvm's: the encoding of every instruction both have,
//    and its alignment;
//  * lib/optable.h: each Imm names Instr constructors of wat.asdl, all of
//    them between them, and an entry's index spaces fit its Imm.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const WASM = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const read = f => readFileSync(join(WASM, f), "utf8");

let bad = 0;
function fail(msg) {
    console.error("optable: " + msg);
    bad++;
}

// ------------------------------------------------------------ the table

const table = [];
for (const m of read("lib/optable.cpp").matchAll(
         /\{ "([^"]+)", 0x(\w+), 0x(\w+), Imm::(\w+), (\d), Space::(\w+), Space::(\w+) \}/g))
    table.push({ name: m[1], prefix: parseInt(m[2], 16), sub: parseInt(m[3], 16), imm: m[4],
                 align: +m[5], x: m[6], y: m[7] });
const byName = new Map();
for (const [i, e] of table.entries()) {
    if (byName.has(e.name))
        fail(`${e.name} twice`);
    byName.set(e.name, e);
    const p = table[i - 1];
    if (p && (p.prefix > e.prefix || (p.prefix === e.prefix && p.sub >= e.sub)))
        fail(`${e.name} is out of order, or has ${p.name}'s encoding`);
}

// ------------------------------------------------------------ the language

const doc = read("Wasm_Assembly_Language.md");
const section = (from, to) => {
    const a = doc.indexOf(from), b = doc.indexOf(to, a);
    if (a < 0 || b < 0)
        throw new Error(`no section '${from}'`);
    return doc.slice(a, b);
};
const ticks = s => [...s.matchAll(/`([^`]+)`/g)].map(m => m[1]);
const need = (text, sentence) => {
    if (!text.includes(sentence))
        throw new Error(`the language no longer says '${sentence}'`);
};

const names = new Set();
const natural = new Map(); // name -> N
const OPS = ["add", "sub", "and", "or", "xor", "xchg", "cmpxchg"];

// Table rows: names in the first cell, N in the column headed N.
function rows(text) {
    let nCol = -1;
    for (const line of text.split("\n")) {
        if (!line.startsWith("|"))
            continue;
        const cells = line.split("|").slice(1, -1).map(c => c.trim());
        if (cells[0] === "Instruction") {
            nCol = cells.indexOf("N");
            continue;
        }
        if (cells[0].startsWith("---"))
            continue;
        let ns = ticks(cells[0]);
        const ops = ns.some(n => n.includes("OP"));
        if (ops)
            ns = ns.flatMap(n => OPS.map(op => n.replace("OP", op)));
        ns.forEach(n => names.add(n));
        if (nCol < 0 || !cells[nCol])
            continue;
        const sizes = cells[nCol].split(",").map(s => +s.trim());
        for (const [i, n] of ns.entries())
            natural.set(n, sizes.length === 1 || ops ? sizes[0] : sizes[i]);
    }
}

// Code blocks: a line's leading "t." words are prefixes for the words after
// them and on the lines below; a word with a dot in it is a whole name.
function lists(text) {
    for (const [, body] of text.matchAll(/```\n([^]*?)```/g)) {
        let prefixes = [];
        for (const line of body.split("\n")) {
            const words = line.trim().split(/\s+/).filter(w => w);
            if (!/^\s/.test(line) && words.length)
                prefixes = [];
            for (const w of words)
                if (w.endsWith(".") && !/^\s/.test(line) && words.indexOf(w) === prefixes.length)
                    prefixes.push(w);
                else if (w.includes("."))
                    names.add(w);
                else
                    prefixes.forEach(p => names.add(p + w));
        }
    }
}

const s75 = section("### 7.5 Every instruction", "## 8. Modules");
rows(s75);
lists(s75);
const blocks = "`block`, `loop`, `if`, `else`, `end` and `try_table` are §7.1.";
need(s75, blocks);
ticks(blocks).filter(n => n !== "else" && n !== "end").forEach(n => names.add(n));
const conv = "for I each of `i32`, `i64`, F each of `f32`, `f64`, and x each of\n" +
             "`s`, `u`: `I.trunc_F_x`, `I.trunc_sat_F_x` and `F.convert_I_x`.";
need(s75, conv);
for (const I of ["i32", "i64"])
    for (const F of ["f32", "f64"])
        for (const x of ["s", "u"])
            [`${I}.trunc_${F}_${x}`, `${I}.trunc_sat_${F}_${x}`, `${F}.convert_${I}_${x}`]
                .forEach(n => names.add(n));

const s101 = section("### 10.1 Threads", "### 10.2");
need(s101, "OP is each of `add`, `sub`, `and`, `or`, `xor`, `xchg` and `cmpxchg`");
rows(s101);
need(s101, "And `atomic.fence`,");
names.add("atomic.fence");
const s102 = section("### 10.2 Legacy exceptions", "### 10.3");
need(s102, "plaininstr  ::= 'rethrow' labelidx");
need(s102, "blockinstr  ::= 'try' label blocktype");
names.add("rethrow").add("try");
ticks(section("### 10.3 Wide arithmetic", "### 10.4")).forEach(n => names.add(n));

for (const n of names)
    if (!byName.has(n))
        fail(`the language has ${n}, the table has not`);
for (const e of table)
    if (!names.has(e.name))
        fail(`the table has ${e.name}, the language has not`);

const MEMS = new Set(["MEM", "MEM_LANE"]);
for (const e of table) {
    const n = natural.get(e.name);
    if (MEMS.has(e.imm) !== (n !== undefined))
        fail(`${e.name}: Imm::${e.imm}, and the language gives ${n === undefined ? "no" : "an"} N`);
    else if (n !== undefined && 1 << e.align !== n)
        fail(`${e.name}: alignment ${1 << e.align}, the language says ${n}`);
    else if (n === undefined && e.align)
        fail(`${e.name}: alignment ${1 << e.align} on no memory access`);
}

// ------------------------------------------------------------ disasm

// llvm's names for what the language calls otherwise.
const LLVM = { "i16x8.load8x8_s": "v128.load8x8_s", "i16x8.load8x8_u": "v128.load8x8_u",
               "i32x4.load16x4_s": "v128.load16x4_s", "i32x4.load16x4_u": "v128.load16x4_u",
               "i64x2.load32x2_s": "v128.load32x2_s", "i64x2.load32x2_u": "v128.load32x2_u" };
const disasm = new Map(); // name -> [{prefix, sub, align}]
for (const m of read("disasm/opcodes.cpp").matchAll(
         /\{ 0x(\w+), 0x(\w+), OpKind::(\w+), (\d), "([^"]+)" \}/g)) {
    const name = LLVM[m[5]] ?? m[5];
    if (!disasm.has(name))
        disasm.set(name, []);
    disasm.get(name).push({ prefix: parseInt(m[1], 16), sub: parseInt(m[2], 16), align: +m[4] });
}
let compared = 0;
const hex = (p, s) => `0x${p.toString(16)} 0x${s.toString(16)}`;
for (const e of table) {
    const d = disasm.get(e.name);
    if (!d)
        continue;
    compared++;
    const same = d.find(o => o.prefix === e.prefix && o.sub === e.sub);
    if (!same)
        fail(`${e.name} is ${hex(e.prefix, e.sub)}, disasm says ${hex(d[0].prefix, d[0].sub)}`);
    else if (same.align !== e.align)
        fail(`${e.name}: alignment ${e.align}, disasm says ${same.align}`);
}

// ------------------------------------------------------------ Imm

const asdl = read("as/wat.asdl");
const instr = asdl.slice(asdl.indexOf("Instr = "), asdl.indexOf("attributes(location loc)",
                                                               asdl.indexOf("Instr = ")));
const ctors = new Set([...instr.replace(/#.*$/gm, "").matchAll(/(?:=|\|)\s*(\w+)/g)].map(m => m[1]));
const header = read("lib/optable.h");
const imms = new Map(); // Imm -> constructors
for (const m of header.slice(header.indexOf("enum class Imm"))
         .matchAll(/^\s+(\w+),\s*\/\/ ([^:\n]+)/gm)) {
    const named = [...m[2].matchAll(/\b[A-Z]\w+/g)].map(w => w[0]).filter(w => ctors.has(w));
    if (!named.length)
        fail(`Imm::${m[1]} names no constructor of Instr: '${m[2]}'`);
    imms.set(m[1], named);
    if (header.slice(header.indexOf("enum class Imm")).indexOf("};") < m.index)
        break;
}
for (const c of ctors)
    if (![...imms.values()].some(n => n.includes(c)))
        fail(`no Imm builds Instr ${c}`);
const used = new Set(table.map(e => e.imm));
for (const i of imms.keys())
    if (!used.has(i))
        fail(`no instruction is Imm::${i}`);

// The index spaces an Imm's immediates count.
const SPACES = { LABEL: 1, BR_TABLE: 1, BR_ON_CAST: 1, IDX: 1, IDX_OPT: 1, NEW_FIXED: 1,
                 CALL_INDIRECT: 1, MEM: 1, MEM_LANE: 1, IDX2: 2, IDX2_OPT: 2, IDX_OPT_IDX: 2 };
for (const e of table) {
    const n = SPACES[e.imm] ?? 0;
    if ((e.x !== "NONE") !== n >= 1 || (e.y !== "NONE") !== n >= 2)
        fail(`${e.name}: Imm::${e.imm} with spaces ${e.x}, ${e.y}`);
}

if (bad)
    process.exit(1);
console.log(`optable ok: ${table.length} instructions, as the language lists them, ` +
            `${natural.size} alignments; ${compared} encodings as disasm has them`);
