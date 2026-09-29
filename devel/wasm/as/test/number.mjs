// as's literals, held to wat2wasm's bits: every i32, i64, f32 and f64
// literal of the test suite, and generated ones — every width's limits,
// halfway points between floats exactly and a hair either side of them, in
// decimal and hex, and random ones. What as refuses, wat2wasm must refuse
// too, and every suite module that expects "constant out of range" must
// have a literal as refuses.
//
// Except hex floats: wat2wasm 1.0.42 truncates some that are a little above
// a halfway point, so those are held to an exact computation here instead,
// and where wat2wasm differs it is counted. f64 decimals are held to JS's
// Number() as well.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";
import { modules, read } from "./wast.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");
const SUITE = join(HERE, "suite/core");
const tmp = mkdtempSync(join(tmpdir(), "number-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

function die(msg) {
    console.error("number: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
{
    const v = spawnSync("wat2wasm", ["--version"], { encoding: "utf8" });
    if (v.error || v.stdout.trim() !== "1.0.42")
        die(`needs wat2wasm 1.0.42: ${v.error?.message ?? v.stdout}`);
}
const bad = [];

// ------------------------------------------------------------ the literals

const TYPES = ["i32", "i64", "f32", "f64"];
const CONST = /\b([if](?:32|64))\.const\s+([^\s()";]+)/g;
const cases = new Map(); // "type literal" -> { type, lit }
const add = (type, lit) => cases.set(`${type} ${lit}`, { type, lit });

// The suite's, in its text and in the text of its quoted modules.
const scripts = readdirSync(SUITE, { recursive: true }).filter((f) => f.endsWith(".wast")).sort();
const malformed = []; // literals of each module that must have one refused
function strings(nodes, out) {
    for (const n of nodes)
        if (n.string)
            out.push(new TextDecoder().decode(n.string));
        else if (n.list)
            strings(n.items, out);
    return out;
}
for (const f of scripts) {
    const src = readFileSync(join(SUITE, f));
    for (const text of [src.toString("latin1"), ...strings(read(src), [])])
        for (const m of text.matchAll(CONST))
            add(m[1], m[2]);
    for (const m of modules(src))
        if (m.command === "assert_malformed" && m.message === "constant out of range") {
            const lits = [...new TextDecoder().decode(m.bytes).matchAll(CONST)];
            if (lits.length)
                malformed.push({ where: `${f}:${m.line}`, lits: lits.map((l) => `${l[1]} ${l[2]}`) });
        }
}
const from_suite = cases.size;

// A generator that is the same every run.
let seed = 0x2545f491;
function rand(n) {
    seed ^= seed << 13, seed ^= seed >>> 17, seed ^= seed << 5;
    return (seed >>> 0) % n;
}
const digits = (n) => Array.from({ length: n }, () => rand(10)).join("");

// Integers: each width's limits, either side, in decimal and hex.
for (const [type, n] of [["i32", 32n], ["i64", 64n]]) {
    for (const v of [0n, 1n, (1n << (n - 1n)) - 1n, 1n << (n - 1n), (1n << n) - 1n, 1n << n]) {
        for (const s of [v.toString(), "0x" + v.toString(16), "0x" + v.toString(16).toUpperCase()]) {
            add(type, s), add(type, "+" + s), add(type, "-" + s);
        }
        add(type, (-v - 1n).toString());
    }
    for (let k = 0; k < 50; k++)
        add(type, (rand(2) ? "-" : "") + digits(1 + rand(22)));
}

// Floats: the exact decimal of a value m * 2^e.
function decimal(m, e) {
    if (e >= 0)
        return (m << BigInt(e)).toString();
    const s = (m * 5n ** BigInt(-e)).toString().padStart(-e + 1, "0");
    return s.slice(0, s.length + e) + "." + s.slice(s.length + e);
}
const FMT = { f32: { p: 24, emin: -126, emax: 127 }, f64: { p: 53, emin: -1022, emax: 1023 } };
for (const [type, f] of Object.entries(FMT)) {
    const p = BigInt(f.p);
    // A float from its exponent field and fraction: m * 2^e.
    const value = (ex, frac) => ex === 0 ? [frac, f.emin - f.p + 1]
                                         : [(1n << (p - 1n)) | frac, ex - f.emax - f.p];
    const points = [[0, 0n], [0, 1n], [0, 2n], [0, (1n << (p - 1n)) - 1n], [1, 0n], [1, 1n],
                    [f.emax, 0n], [f.emax, 1n], [2 * f.emax, (1n << (p - 1n)) - 1n],
                    [2 * f.emax, (1n << (p - 1n)) - 2n]];
    for (let k = 0; k < 60; k++)
        points.push([rand(2 * f.emax + 1), BigInt(rand(1 << 30)) * BigInt(rand(1 << 30)) %
                                           (1n << (p - 1n))]);
    for (const [ex, frac] of points) {
        const [m, e] = value(ex, frac);
        // The value, and the halfway point above it: 2m+1 at e-1.
        for (const [mm, ee] of [[m, e], [2n * m + 1n, e - 1]]) {
            const d = decimal(mm, ee);
            add(type, d);
            add(type, "-" + d);
            add(type, d + "000001"); // a hair above
            if (d.includes(".") && /[1-9]$/.test(d)) // a hair below
                add(type, d.slice(0, -1) + (Number(d.at(-1)) - 1) + "9999999999");
            add(type, `0x${mm.toString(16)}p${ee}`);
            add(type, `0x${(mm * 4n + 1n).toString(16)}p${ee - 2}`);
            add(type, `0x${(mm * 4n - 1n).toString(16)}p${ee - 2}`);
        }
    }
    // Random decimals and hex floats, around 1 and near the limits.
    const range = f.emax === 127 ? 40 : 310;
    for (let k = 0; k < 150; k++) {
        const e = rand(2 * range + 20) - range - 10;
        add(type, `${digits(1 + rand(3))}.${digits(rand(25))}e${e}`);
        add(type, `0x${rand(1 << 30).toString(16)}.${rand(1 << 30).toString(16)}p${rand(2 * f.emax + 80) - f.emax - 40}`);
    }
    for (const s of ["0", "-0", "1", "-0x1", "inf", "-inf", "+inf", "nan", "-nan", "+nan", "nan:0x1",
                     "nan:0x3f_ffff", "nan:0x40_0000", "nan:0x7f_ffff", "nan:0x80_0000",
                     "nan:0xf_ffff_ffff_ffff", "nan:0x10_0000_0000_0000", "nan:0x0", "1e1000000",
                     "1e-1000000", "0x1p1000000", "0x1p-1000000", "1e99999999999999999999",
                     "0." + "0".repeat(400) + "1", "1" + "0".repeat(400), "0x1" + "0".repeat(300),
                     "1_000.000_1", "0x1_F.F_8p1_0"])
        add(type, s);
}

// ------------------------------------------------------------ as's answers

const list = [...cases.values()];
const as = await assembler(AS);
const r = as.run(["--numbers", "n.txt"], { "n.txt": list.map((c) => `${c.type} ${c.lit}`).join("\n") });
if (r.status !== 0 || r.err)
    die(`as --numbers: status ${r.status}: ${r.err}`);
const lines = r.out.split("\n").filter((l) => l);
if (lines.length !== list.length)
    die(`as --numbers printed ${lines.length} lines for ${list.length} literals`);
const got = new Map();
list.forEach((c, k) => {
    const want = `${c.type} ${c.lit} `;
    if (!lines[k].startsWith(want))
        die(`line ${k + 1} is ${lines[k]}, expected ${want}…`);
    got.set(`${c.type} ${c.lit}`, lines[k].slice(want.length));
});

// ------------------------------------------------------------ wat2wasm's

function wat2wasm(name, lits) {
    const text = "(module (func\n" + lits.map((c) => `${c.type}.const ${c.lit} drop\n`).join("") + "))\n";
    writeFileSync(join(tmp, name + ".wat"), text);
    return spawnSync("wat2wasm", [name + ".wat", "-o", name + ".wasm"], { cwd: tmp, encoding: "utf8" });
}

// The immediates of `T.const x drop …` in the one function's body.
function immediates(bytes) {
    let i = 8;
    const leb = () => {
        let v = 0n, s = 0n, b;
        do {
            b = bytes[i++];
            v |= BigInt(b & 0x7f) << s;
            s += 7n;
        } while (b & 0x80);
        return [v, s, b];
    };
    while (bytes[i] !== 10) {
        i++;
        const [n] = leb();
        i += Number(n);
    }
    i++;
    leb(), leb(), leb(); // section size, one function, its size
    if (bytes[i++] !== 0)
        die("wat2wasm wrote locals");
    const out = [];
    const hex = (n) => {
        let v = 0n;
        for (let k = n - 1; k >= 0; k--)
            v = v << 8n | BigInt(bytes[i + k]);
        i += n;
        return v;
    };
    for (;;) {
        const op = bytes[i++];
        if (op === 0x0b)
            return out;
        if (op === 0x41 || op === 0x42) {
            const [v, s, b] = leb();
            const w = op === 0x41 ? 32n : 64n;
            const signed = b & 0x40 ? v - (1n << s) : v;
            out.push(BigInt.asUintN(Number(w), signed));
        } else if (op === 0x43 || op === 0x44) {
            out.push(hex(op === 0x43 ? 4 : 8));
        } else {
            die(`wat2wasm wrote opcode ${op}`);
        }
        if (bytes[i++] !== 0x1a)
            die("no drop");
    }
}

// A hex float rounded exactly, as as prints it; null for another literal.
function exact_hex(type, lit) {
    const m = /^([+-]?)0x([0-9a-fA-F_]+)(?:\.([0-9a-fA-F_]*))?(?:[pP]([+-]?[0-9_]+))?$/.exec(lit);
    if (!m)
        return null;
    const f = FMT[type], p = BigInt(f.p), width = type === "f32" ? 32n : 64n;
    const frac = (m[3] ?? "").replace(/_/g, "");
    let mant = BigInt("0x" + m[2].replace(/_/g, "") + frac);
    let e = BigInt((m[4] ?? "0").replace(/_/g, "")) - 4n * BigInt(frac.length);
    const sign = m[1] === "-" ? 1n << (width - 1n) : 0n;
    const hex = (v) => "0x" + (sign | v).toString(16).padStart(Number(width / 4n), "0");
    if (mant === 0n)
        return hex(0n);
    const top = e + BigInt(mant.toString(2).length) - 1n; // the leading bit's exponent
    if (top > BigInt(f.emax))
        return "out of range";
    if (top < BigInt(f.emin) - p - 2n)
        return hex(0n);
    let u = (top > BigInt(f.emin) ? top : BigInt(f.emin)) - (p - 1n);
    let q;
    if (u <= e) {
        q = mant << (e - u);
    } else {
        const sh = u - e, rem = mant & ((1n << sh) - 1n), half = 1n << (sh - 1n);
        q = mant >> sh;
        if (rem > half || (rem === half && (q & 1n)))
            q++;
    }
    if (q === 1n << p)
        q >>= 1n, u++;
    if (q < 1n << (p - 1n))
        return hex(q);
    const ex = u + p - 1n;
    if (ex > BigInt(f.emax))
        return "out of range";
    return hex((ex + BigInt(f.emax)) << (p - 1n) | (q - (1n << (p - 1n))));
}

// An f64 decimal by JS's own conversion, which is exact.
function js_f64(lit) {
    if (!/^[+-]?[0-9_]+(\.[0-9_]*)?([eE][+-]?[0-9_]+)?$/.test(lit))
        return null;
    const v = Number(lit.replace(/_/g, ""));
    if (!Number.isFinite(v))
        return "out of range";
    const b = new DataView(new ArrayBuffer(8));
    b.setFloat64(0, v);
    return "0x" + b.getBigUint64(0).toString(16).padStart(16, "0");
}

let hex_checked = 0, js_checked = 0;
const wabt_wrong = [];
for (const c of list) {
    if (c.type[0] !== "f")
        continue;
    const mine = got.get(`${c.type} ${c.lit}`);
    if (!/^(0x|out of range)/.test(mine))
        continue; // not a literal the lexer takes
    const x = exact_hex(c.type, c.lit);
    if (x !== null) {
        hex_checked++;
        if (mine !== x)
            bad.push(`${c.type} ${c.lit}: as ${mine}, exactly ${x}`);
    }
    const j = c.type === "f64" ? js_f64(c.lit) : null;
    if (j !== null) {
        js_checked++;
        if (mine !== j)
            bad.push(`${c.type} ${c.lit}: as ${mine}, Number() ${j}`);
    }
}

const numeric = list.filter((c) => /^(0x|out of range)/.test(got.get(`${c.type} ${c.lit}`)));
const valid = numeric.filter((c) => got.get(`${c.type} ${c.lit}`).startsWith("0x"));
const refused = numeric.filter((c) => got.get(`${c.type} ${c.lit}`) === "out of range");
{
    const w = wat2wasm("valid", valid);
    if (w.status !== 0) {
        bad.push(`wat2wasm refuses literals as takes:\n${w.stderr.split("\n").slice(0, 20).join("\n")}`);
    } else {
        const imm = immediates(readFileSync(join(tmp, "valid.wasm")));
        valid.forEach((c, k) => {
            const width = c.type.endsWith("32") ? 8 : 16;
            const want = "0x" + imm[k].toString(16).padStart(width, "0");
            const mine = got.get(`${c.type} ${c.lit}`);
            if (mine === want)
                return;
            if (c.type[0] === "f" && exact_hex(c.type, c.lit) !== null)
                wabt_wrong.push(`${c.type} ${c.lit}`);
            else
                bad.push(`${c.type} ${c.lit}: as ${mine}, wat2wasm ${want}`);
        });
    }
}
// wabt reports each line's error, so one run shows which it takes.
{
    const w = wat2wasm("refused", refused);
    const lines = new Set([...w.stderr.matchAll(/^refused\.wat:(\d+):/gm)].map((m) => Number(m[1])));
    refused.forEach((c, k) => {
        if (!lines.has(k + 2))
            bad.push(`${c.type} ${c.lit}: as refuses it, wat2wasm takes it`);
    });
}
for (const m of malformed)
    if (!m.lits.some((l) => got.get(l) === "out of range"))
        bad.push(`${m.where}: as takes every literal of ${m.lits.join(", ")}`);

const suite_wrong = wabt_wrong.filter((k) => [...cases.keys()].indexOf(k) < from_suite);
if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 40).join("\n  "));
console.log(`number ok: ${list.length} literals (${from_suite} of the suite), ${refused.length} ` +
            `out of range; ${hex_checked} hex floats exact, ${js_checked} f64 as Number() has them, ` +
            `the rest as wat2wasm has them; ${malformed.length} malformed modules refused. ` +
            `wat2wasm rounds ${wabt_wrong.length} hex floats wrongly, ${suite_wrong.length} of the ` +
            `suite's${suite_wrong.length ? ": " + suite_wrong.join(", ") : ""}`);
