// disasm, held to llvm-objdump line for line where it disassembles code:
// every fixture's objects, archives and reference program, each program
// stripped, the SDK's libraries, ld.wasm, and modules made here holding every
// opcode llvm decodes, under -d, -dr, -dC and --no-show-raw-insn. Its layout,
// addresses, bytes and relocations are llvm's; its instructions are llvm's
// once llvm's spelling is brought to disasm's. Its comments are checked on
// their own: branches against a model of the blocks, names against the
// relocations and the functions' labels. Then the data: its rows must give
// back each segment's bytes, at its address, and a golden file shows what an
// object looks like whole. Then the errors.

import { execFileSync, spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync }
    from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { boot, get, manifest, plant, run } from "../../ld/test/wasmlib.mjs";
import { foreign } from "../../nm/test/foreign.mjs";

function die(msg) {
    console.error("disasm: " + msg);
    process.exit(1);
}

const HERE = dirname(fileURLToPath(import.meta.url));
const m = manifest();
const DISASM = join(dirname(m.ld), "../disasm/disasm.wasm");
const OBJDUMP = m.objdump;
const LLVM_STRIP = join(dirname(m.objdump), "llvm-strip");
const tmp = mkdtempSync(join(tmpdir(), "disasm-"));
process.on("exit", () => process.env.KEEP ? console.log("kept " + tmp) : rmSync(tmp, { recursive: true, force: true }));

const H = await boot();
plant(H, "/bin/disasm", new Uint8Array(readFileSync(DISASM)));
const bad = [];

// Every input in one directory, on the host and in /tmp/w alike, so a
// relative name reads the same in both tools' output.
const dir = join(tmp, "w");
mkdirSync(dir);
H.store.dirs.add("/tmp/w");
function add(name, bytes) {
    writeFileSync(join(dir, name), bytes);
    plant(H, "/tmp/w/" + name, new Uint8Array(bytes));
    return name;
}

// A command line of any length, through a script, in /tmp/w; stdout,
// stderr, status.
function sh(cmd) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
        H.store.files.delete(k);
    plant(H, "/tmp/c", `cd /tmp/w; ${cmd} >/tmp/o 2>/tmp/e; echo $? >/tmp/s\n`);
    run(H, "sh /tmp/c");
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "", status: Number(get(H, "/tmp/s")) };
}

function llvm(args) {
    const r = spawnSync(OBJDUMP, args, { cwd: dir, encoding: "utf8", maxBuffer: 1 << 30 });
    if (r.error)
        die(`llvm-objdump ${args.join(" ")}: ${r.error.message}`);
    return { out: r.stdout, err: r.stderr, status: r.status };
}

// The first line where two outputs part.
function differ(a, b) {
    const x = a.split("\n"), y = b.split("\n");
    for (let i = 0; i < Math.max(x.length, y.length); i++)
        if (x[i] !== y[i])
            return `line ${i + 1}: ${JSON.stringify(x[i])}, llvm-objdump ${JSON.stringify(y[i])}`;
    return "";
}

// ------------------------------------------------------------ llvm's spelling

const INSN = /^( *[0-9a-f]+:[^\t]*\t)(.*)$/;
const isInsn = (line) => !line.startsWith("\t") && !line.includes("\tfile format wasm") && INSN.test(line);
const isNote = (line) => /^ {20,}# /.test(line);

// The bytes of an instruction line, where they are shown.
const rawBytes = (prefix) => (prefix.slice(prefix.indexOf(":") + 1).match(/[0-9a-f]{2}/g) ?? []).map((h) => parseInt(h, 16));

function uleb(b, p) {
    let v = 0n, s = 0n, x;
    do {
        x = b[p.at++];
        v |= BigInt(x & 0x7f) << s;
        s += 7n;
    } while (x & 0x80);
    return v;
}

// An operand llvm printed as an int64_t that is unsigned.
const unsigned = (t) => t.replace(/-\d+/g, (n) => String(BigInt.asUintN(64, BigInt(n))));

// A hex float as llvm prints it, as a number.
function hexFloat(t) {
    const m = /^(-?)0x([0-9a-f])(?:\.([0-9a-f]+))?p(-?\d+)$/.exec(t);
    if (!m)
        return NaN;
    let v = parseInt(m[2], 16);
    const frac = m[3] ?? "";
    for (let i = 0; i < frac.length; i++)
        v += parseInt(frac[i], 16) / 16 ** (i + 1);
    return (m[1] ? -1 : 1) * v * 2 ** Number(m[4]);
}

// The float constant at the end of the bytes, as disasm prints it but for
// the digits of a finite one, which are checked by value.
function floatText(bytes, width) {
    const b = new Uint8Array(bytes.slice(bytes.length - width));
    const dv = new DataView(b.buffer);
    const bits = width === 4 ? BigInt(dv.getUint32(0, true)) : dv.getBigUint64(0, true);
    const mb = width === 4 ? 23n : 52n, eb = width === 4 ? 8n : 11n;
    const man = bits & ((1n << mb) - 1n), exp = (bits >> mb) & ((1n << eb) - 1n);
    const sign = bits >> (mb + eb) ? "-" : "";
    if (exp !== (1n << eb) - 1n)
        return { value: width === 4 ? dv.getFloat32(0, true) : dv.getFloat64(0, true) };
    if (!man)
        return { text: sign + "inf" };
    return { text: sign + (man === 1n << (mb - 1n) ? "nan" : "nan:0x" + man.toString(16)) };
}

const MEMARG = /(\.load|\.store|\.atomic\.|^memory\.atomic\.)/;
const SIGNED = /^(i32\.const|i64\.const|memory\.init|data\.drop|memory\.copy|memory\.fill)$/;
const BLOCKS = /^(block|loop|if|try|try_table)$/;

// llvm's text of an instruction, spelled as disasm spells it; or a check,
// for what is compared otherwise. `prefix` has the bytes, where shown.
function respell(text, prefix, raw) {
    if (text === "<unknown>")
        return text;
    // Most names end at a tab, ref.func's, ref.test's and ref.cast's at a space.
    const m0 = /^([^\t ]+)[\t ]*(.*)$/.exec(text);
    let name = m0[1];
    let ops = m0[2].trim().replace(/ +/g, " ");
    if (name === "f32.select")
        name = "select";
    // Not value types, which llvm names all the same.
    if (name === "select" || BLOCKS.test(name))
        ops = ops.replace(/\b(void|func)\b/g, "invalid_type");
    if (name.startsWith("ref.null_")) {
        ops = name.slice(9);
        name = "ref.null";
    }
    if (MEMARG.test(name) && name !== "atomic.fence") {
        const m = /^(acqrel )?(-?\d+)(?::p2align=(\d+))?(?:, (\d+))?$/.exec(ops);
        const parts = [];
        if (m[1])
            parts.push("acqrel");
        const off = BigInt.asUintN(64, BigInt(m[2]));
        if (off)
            parts.push(`offset=${off}`);
        if (m[3] !== undefined)
            parts.push(Number(m[3]) < 64 ? `align=${2n ** BigInt(m[3])}` : `align=2**${m[3]}`);
        if (m[4] !== undefined)
            parts.push(m[4]);
        ops = parts.join(" ");
    } else if (name === "call_indirect" || name === "return_call_indirect") {
        if (!raw)
            return { name, ops: new RegExp(`^type=${unsigned(ops)} table=\\d+$`) };
        const b = rawBytes(prefix), p = { at: 1 };
        const type = uleb(b, p), table = uleb(b, p);
        ops = `type=${type} table=${table}`;
    } else if (name === "f32.const" || name === "f64.const") {
        if (!raw && /inf|nan/.test(ops)) {
            // An f32's NaN payload was widened, its quiet bit set.
            const m = /^(-?)(infinity|nan)(?::0x([0-9a-f]+))?$/.exec(ops);
            if (m[2] === "infinity")
                return `${name}\t${m[1]}inf`;
            if (!m[3] || name === "f64.const")
                return `${name}\t${ops}`;
            const p = BigInt("0x" + m[3]) >> 29n, q = p & ~(1n << 22n);
            const alt = [p, q].map((v) => v === 1n << 22n ? "nan" : "nan:0x" + v.toString(16));
            return { name, ops: new RegExp(`^${m[1]}(${alt.join("|")})$`) };
        }
        if (!raw)
            return { name, float: hexFloat(ops), f32: name === "f32.const", llvm: ops };
        const f = floatText(rawBytes(prefix), name === "f32.const" ? 4 : 8);
        if (f.text !== undefined)
            ops = f.text;
        else
            return { name, float: f.value, f32: name === "f32.const", llvm: ops };
    } else if (BLOCKS.test(name) && ops.startsWith("unknown_type")) {
        return { name, rest: ops.slice(12).trim() };
    } else if (!SIGNED.test(name)) {
        ops = unsigned(ops);
    }
    return ops ? `${name}\t${ops}` : name;
}

// Whether disasm's text of an instruction is llvm's, respelled.
function agrees(ours, want, notes) {
    if (typeof want === "string")
        return ours === want;
    const tab = ours.indexOf("\t");
    const name = tab < 0 ? ours : ours.slice(0, tab), ops = tab < 0 ? "" : ours.slice(tab + 1);
    if (name !== want.name)
        return false;
    if (want.ops)
        return want.ops.test(ops);
    if (want.rest !== undefined) {
        const k = ops.search(/ \((catch|\d)/);
        const type = k < 0 ? ops : ops.slice(0, k), clauses = k < 0 ? "" : ops.slice(k + 1);
        return /->|^type=\d+$/.test(type) && clauses === want.rest;
    }
    // A float: its value, the fewest digits, and llvm's hex in the comment.
    const v = Number(ops);
    const same = want.f32 ? Math.fround(v) === want.float : v === want.float;
    const hex = want.float === 0 ? notes.length === 0 : notes[0] === want.llvm;
    return same && hex && (Object.is(v, -0) === Object.is(want.float, -0)) && /^-?[0-9.e+-]+$/.test(ops);
}

// Where disasm's code parts from llvm-objdump's, beside what it adds.
function against(got, want, raw) {
    const x = [], xn = [];
    for (const line of got.split("\n")) {
        if (isNote(line)) {
            xn[x.length - 1].push(line.trim().slice(2));
            continue;
        }
        const k = line.indexOf(" # ");
        if (isInsn(line) && k >= 0) {
            x.push(line.slice(0, k).trimEnd());
            xn.push([line.slice(k + 3)]);
        } else {
            x.push(line);
            xn.push([]);
        }
    }
    const y = want.split("\n").filter((l) => !isNote(l));
    for (let i = 0; i < Math.max(x.length, y.length); i++) {
        const a = x[i] ?? "", b = isInsn(y[i] ?? "") ? y[i].replace(/ +# .*$/, "") : y[i] ?? "";
        if (!isInsn(b) || !isInsn(a)) {
            if (a !== b)
                return `line ${i + 1}: ${JSON.stringify(a)}, llvm-objdump ${JSON.stringify(b)}`;
            continue;
        }
        const [, pa, ta] = INSN.exec(a), [, pb, tb] = INSN.exec(b);
        const notes = xn[i].filter((n) => /^-?0x[0-9a-f.]+p-?\d+$/.test(n));
        if (pa !== pb || !agrees(ta, respell(tb.trimEnd(), pb, raw), notes))
            return `line ${i + 1}: ${JSON.stringify(a)}, llvm-objdump ${JSON.stringify(b)}`;
    }
    return "";
}

// ------------------------------------------------------------ the comments

// Every branch comment of a listing, against a model of the blocks: each
// function's labels count from 0, a block's lands at its end and a loop's
// at its start.
function flow(out, what) {
    let stack = [], counter = 0, inCode = false;
    const lines = out.split("\n");
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        if (line.startsWith("Disassembly of section "))
            inCode = line === "Disassembly of section CODE:";
        if (/^[0-9a-f]{8,16} </.test(line)) {
            stack = [];
            counter = 0;
            continue;
        }
        if (!inCode || !isInsn(line))
            continue;
        const k = line.indexOf(" # ");
        const have = [];
        if (k >= 0)
            have.push(line.slice(k + 3));
        while (i + 1 < lines.length && isNote(lines[i + 1]))
            have.push(lines[++i].trim().slice(2));
        const text = INSN.exec(k >= 0 ? line.slice(0, k).trimEnd() : line)[2];
        const tab = text.indexOf("\t");
        const name = tab < 0 ? text : text.slice(0, tab), ops = tab < 0 ? "" : text.slice(tab + 1);
        const want = [];
        const seen = new Set();
        const frame = (d) => stack[stack.length - 1 - d];
        const branch = (d) => {
            if (seen.has(d))
                return;
            seen.add(d);
            const f = frame(d);
            want.push(f ? `${d}: ${f.kind === "loop" ? "up" : "down"} to label${f.label}`
                        : d === stack.length ? `${d}: return` : `${d}: invalid depth`);
        };
        switch (name) {
        case "block": case "loop": case "if": case "try":
            if (name === "loop")
                want.push(`label${counter}:`);
            stack.push({ label: counter++, kind: name, eh: "" });
            break;
        case "try_table":
            for (const m of ops.matchAll(/\((?:catch(?:_ref)? \d+ |catch_all(?:_ref)? )?(\d+)\)/g))
                branch(Number(m[1]));
            stack.push({ label: counter++, kind: name, eh: "" });
            break;
        case "end": {
            const f = stack.pop();
            if (f && f.kind !== "loop")
                want.push(`label${f.label}:`);
            break;
        }
        case "br": case "br_if":
            branch(Number(ops));
            break;
        case "br_table":
            for (const d of ops.slice(1, -1).split(", "))
                branch(Number(d));
            break;
        case "catch": case "catch_all": {
            const f = stack[stack.length - 1];
            if (!f || f.kind !== "try")
                want.push("catch without try");
            else if (f.eh === "all")
                want.push("catch after catch_all");
            else {
                f.eh = name === "catch" ? "one" : "all";
                want.push(`catch${f.label}:`);
            }
            break;
        }
        case "rethrow": {
            const d = Number(ops), f = frame(d);
            want.push(f && f.kind === "try" && f.eh ? `${d}: from catch${f.label}`
                                                     : `${d}: ${f ? "not a catch" : "invalid depth"}`);
            break;
        }
        case "delegate": {
            const top = stack[stack.length - 1];
            if (!top || top.kind !== "try") {
                want.push("delegate without try");
                break;
            }
            stack.pop();
            want.push(`label${top.label}:`);
            const d = Number(ops), f = frame(d);
            want.push(f ? `${d}: ${f.kind === "try" ? "to catch" : "out of label"}${f.label}`
                        : d === stack.length ? `${d}: to caller` : `${d}: invalid depth`);
            break;
        }
        }
        const FLOW = /^(\d+: (up to|down to|return|invalid depth|from catch|not a catch|to catch|out of label|to caller)|label\d+:$|catch\d+:$|catch without try$|catch after catch_all$|delegate without try$)/;
        const got = have.filter((n) => FLOW.test(n));
        if (got.join("|") !== want.join("|")) {
            bad.push(`${what}: line ${i + 1}: ${JSON.stringify(line)}: ${JSON.stringify(got)}, model ${JSON.stringify(want)}`);
            return;
        }
    }
}

// Each call's and global's name, against the relocation beneath it.
function names(out, what) {
    const lines = out.split("\n");
    let checked = 0;
    for (let i = 0; i + 1 < lines.length; i++) {
        const m = /\t(call|return_call|global\.get|global\.set)\t\d+ +# (.*)$/.exec(lines[i]);
        const r = /^\t+[0-9a-f]+:  R_WASM_(FUNCTION|GLOBAL)_INDEX_LEB\t(.*)\+0$/.exec(lines[i + 1]);
        if (!r)
            continue;
        checked++;
        if (!m || m[2] !== r[2]) {
            bad.push(`${what}: line ${i + 1}: ${JSON.stringify(lines[i])}, relocation ${r[2]}`);
            return 0;
        }
    }
    return checked;
}

// A program's calls, against the label of the function called: its
// defined functions are its chunks, in order, when all are named.
function calls(out, what, imports) {
    const labels = [], lines = out.split("\n");
    for (const l of lines) {
        const m = /^[0-9a-f]{8} <(.*)>:$/.exec(l);
        if (m && m[1] !== "CODE")
            labels.push(m[1]);
    }
    let checked = 0;
    for (const l of lines) {
        const m = /\tcall\t(\d+) +# (.*)$/.exec(l);
        if (!m || Number(m[1]) < imports)
            continue;
        checked++;
        if (labels[Number(m[1]) - imports] !== m[2]) {
            bad.push(`${what}: ${JSON.stringify(l)}: function ${m[1]} is ${labels[Number(m[1]) - imports]}`);
            return 0;
        }
    }
    return checked;
}

// The functions a module imports.
function imported(bytes) {
    const b = new Uint8Array(bytes);
    let p = 8, n = 0;
    const u = () => {
        let v = 0, s = 0, x;
        do {
            x = b[p++];
            v += (x & 0x7f) * 2 ** s;
            s += 7;
        } while (x & 0x80);
        return v;
    };
    while (p < b.length) {
        const id = b[p++], size = u(), end = p + size;
        if (id === 2)
            for (let k = u(); k--;) {
                for (let j = 0; j < 2; j++) {
                    const len = u();
                    p += len;
                }
                const kind = b[p++];
                if (kind === 0) {
                    n++;
                    u();
                } else if (kind === 1) {
                    p++;
                    const f = b[p++];
                    u();
                    if (f & 1)
                        u();
                } else if (kind === 2) {
                    const f = b[p++];
                    u();
                    if (f & 1)
                        u();
                } else if (kind === 3) {
                    p += 2;
                } else {
                    p++;
                    u();
                }
            }
        p = end;
    }
    return n;
}

// ------------------------------------------------------------ modules by hand

const leb = (n) => {
    const b = [];
    do {
        let x = n & 0x7f;
        n >>>= 7;
        b.push(n ? x | 0x80 : x);
    } while (n);
    return b;
};
const str = (s) => [...leb(s.length), ...new TextEncoder().encode(s)];
const vec = (items) => [...leb(items.length), ...items.flat()];
const section = (id, body) => [id, ...leb(body.length), ...body];
const sub = (id, body) => [id, ...leb(body.length), ...body];

// A program of one function per body, each named, so llvm starts afresh at
// each: the bytes run on into the next function where an instruction does.
function program(bodies, data = []) {
    const code = bodies.map((b) => [...leb(b.length + 1), 0, ...b]);
    const names = bodies.map((_, i) => [...leb(i), ...str("f" + i)]);
    return new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        ...section(1, vec([[0x60, 0, 0]])),
        ...section(3, vec(bodies.map(() => [0]))),
        ...section(10, vec(code)),
        ...(data.length ? section(11, vec(data)) : []),
        ...section(0, [...str("name"), ...sub(1, vec(names))])]);
}

// Every opcode, the one-byte ones and each prefix's first 320, followed by
// `fill`, for llvm to read what operands it will out of.
function opcodes(fill) {
    const keys = [];
    for (let b = 0; b < 256; b++)
        if (b < 0xfb || b == 0xff)
            keys.push([b]);
    for (const p of [0xfb, 0xfc, 0xfd, 0xfe])
        for (let s = 0; s < 0x140; s++)
            keys.push([p, ...leb(s)]);
    return program(keys.map((k) => [...k, ...fill, 0x0b]));
}

const count = (n) => Array.from({ length: n }, (_, i) => i + 1);
const inputs = [];
const FILLS = [count(20), [0, ...count(20)], [2, 0x80, 1, ...count(18)], [3, 0xff, 0x7f, ...count(17)],
               [4, 0x40, ...count(18)], [0x40, 0x7f, 0, 0, 0x80, 0x7f, ...count(14)],
               [1, 0x7e, 0xff, 0xff, 0xff, 0x7f, ...count(14)], [0x80, 1, 0x6f, 0x69, 0x60, ...count(15)],
               [5, 1, 5, 1, 2, 3, 4, 0x81, 0x80, 0, 5, 6], Array(20).fill(0), Array(20).fill(0x40)];
// No alignment has 0x20 set: llvm-objdump crashes on it outside an atomic.
FILLS.forEach((f, i) => inputs.push(add(`ops${i}.wasm`, opcodes(f))));

// What the fills leave out: typed select, ref.null, atomic orderings,
// try_table's catches, the legacy exception labels, branch annotations,
// block types, floats of every class, the widest LEBs, and bytes past the
// end of the section.
const f32 = (x) => [0x43, ...new Uint8Array(new Float32Array([x]).buffer)];
const f32bits = (u) => [0x43, u & 0xff, (u >> 8) & 0xff, (u >> 16) & 0xff, u >>> 24];
const f64 = (x) => [0x44, ...new Uint8Array(new Float64Array([x]).buffer)];
const f64bits = (hi, lo) => [0x44, ...new Uint8Array(new Uint32Array([lo, hi]).buffer)];
const SPECIAL = [
    [0x1c, 1, 0x7f], [0x1c, 1, 0x40], [0x1c, 2, 0x7f, 0x7e], [0x1c, 0], [0x1c, 1, 0xff, 0], [0x1c, 0x81, 0, 0x7f],
    [0xd0, 0x70], [0xd0, 0x6f], [0xd0, 0x69], [0xd0, 0x40], [0xd0, 0xf0, 0x7f],
    [0xfe, 3, 0], [0xfe, 3, 1], [0xfe, 3, 2], [0xfe, 0x10, 0x22, 0, 5], [0xfe, 0x10, 0x22, 1, 5],
    [0xfe, 0x10, 0x20, 5, 5], [0xfe, 0x48, 0x62, 1, 4], [0xfe, 0, 0x22, 1, 4], [0xfe, 1, 0x20, 1, 4],
    [0x1f, 0x40, 2, 0, 0, 1, 1, 0, 2, 2, 3, 3, 0x0b], [0x1f, 0x40, 0, 0x0b], [0x1f, 0x7f, 1, 1, 0, 0, 0x0b],
    [0x1f, 0x40, 1, 4, 0, 0], [0x1f, 0x40, 1, 0x80, 0], [0x1f, 0x40, 1, 0, 0x80, 0x80, 0x80, 0x80, 0x10, 0],
    [6, 0x40, 7, 0, 0x19, 0x0b], [6, 0x40, 0x18, 0], [2, 0x40, 6, 0x40, 0x18, 1, 0x0b], [9, 0],
    [6, 0x40, 9, 0, 7, 1, 9, 0, 0x0b], [6, 0x40, 0x19, 0x19, 7, 0], [3, 0x40, 6, 0x40, 0x18, 1],
    [0x19], [7, 0], [0x18, 0],
    [2, 0x40, 3, 0x40, 0x0c, 0, 0x0c, 1, 0x0c, 2, 0x0d, 5, 0x0e, 3, 1, 1, 0, 1, 0x0b],
    [2, 0x40, 2, 0x7f, 2, 0x7e, 0x0e, 3, 0, 1, 2, 5, 0x0b], [0x0e, 0, 0], [0x0c, 9],
    [2, 0x80, 0x7f], [2, 0xff, 0], [2, 0x80, 1], [2, 0x7b], [2, 0x70], [2, 0x6f], [2, 0x60], [2, 0x69], [4, 0x40, 5, 0x0b],
    f32(1), f32(0), f32(-0), f32(0.1), f32(-3.5e38), f32bits(1), f32bits(0x7f800000), f32bits(0xff800000),
    f32bits(0x7fc00000), f32bits(0x7f800001), f32bits(0xffc00000), f32bits(0x7fc00001), f32bits(0x807fffff),
    f64(1), f64(0.1), f64(-2.5), f64(1e300), f64(Math.PI), f64bits(0, 1), f64bits(0x7ff80000, 0),
    f64bits(0x7ff00000, 1), f64bits(0xfff00000, 0), f64bits(0xfff80000, 0), f64bits(0x800fffff, 0xffffffff),
    [0x41, 0x80, 0x80, 0x80, 0x80, 0x78], [0x42, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f],
    [0x42, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00], [0x42, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f],
    [0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01], [0x20, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00],
    [0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02],
    [0x28, 2, 0], [0x28, 0, 0], [0x28, 3, 0], [0x28, 0x40, 0], [0x29, 3, 4], [0x2c, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], [1, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0],
    Array(40).fill(0), [0x11, 0, 0], [0x11, 1, 1], [0x13, 1, 0], [0x20], [0x41], [0xfd], [0xfd, 0x80],
    [0xfc, 0x80, 0x80, 0x80, 0x80, 0x10], [0xfd, 0x80, 0x80, 0x80, 0x80, 0x80, 4],
    [0xfd, 0x0c, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0x0b, 0x0b],
    [0x44, 1, 2],
];
inputs.push(add("special.wasm", program(SPECIAL)));

// Locals of every type, and a data segment past a passive one.
{
    const code = [[2, 0, 0x0b], [...leb(12), 5, 1, 0x7f, 2, 0x7e, 1, 0x7d, 1, 0x7c, 3, 0x7b, 0x0b]];
    const m = new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        ...section(1, vec([[0x60, 0, 0]])), ...section(3, vec([[0], [0]])),
        ...section(10, vec(code)),
        ...section(11, vec([[1, ...str("passive bytes")],
                            [0, 0x41, ...leb(1024), 0x0b, ...str("hello, world\n"), ],
                            [0, 0x41, ...leb(2048), 0x0b, ...leb(100), ...Array(64).fill(0), ...count(36)]])),
        ...section(0, [...str("name"), ...sub(1, vec([[0, ...str("f")], [1, ...str("g")]])),
                       ...sub(9, vec([[0, ...str(".passive")], [1, ...str(".rodata")]]))])]);
    inputs.push(add("locals.wasm", m));
}

// A name section naming some functions, where the exports go unused; and
// exports alone, two naming one function. Every function starts a chunk.
{
    const mk = (names, exps) => new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        ...section(1, vec([[0x60, 0, 0]])), ...section(3, vec([[0], [0], [0], [0]])),
        ...section(7, vec(exps.map(([n, i]) => [...str(n), 0, i]))),
        ...section(10, vec([[3, 0, 1, 0x0b], [3, 0, 1, 0x0b], [3, 0, 1, 0x0b], [3, 0, 1, 0x0b]])),
        ...(names ? section(0, [...str("name"), ...sub(1, vec(names.map(([i, n]) => [...leb(i), ...str(n)])))])
                  : [])]);
    inputs.push(add("names.wasm", mk([[0, "a"], [2, "zz"]], [["ex", 2], ["ey", 3], ["b", 1]])));
    inputs.push(add("exports.wasm", mk(null, [["ex", 2], ["ey", 2]])));
}

// ------------------------------------------------------------ the corpus

const corpus = [add("ld.wasm", readFileSync(m.ld))];
let n = 0;
const seen = new Set();
const strip = (from, to) => execFileSync(LLVM_STRIP, [from, "-o", to]);
const standalone = []; // modules whose data is checked row by row
for (const [name, fx] of Object.entries(m.fixtures)) {
    standalone.push(add(`fx_${name}.wasm`, readFileSync(fx.reference)));
    strip(fx.reference, join(tmp, "s.wasm"));
    corpus.push(add(`fx_${name}.stripped`, readFileSync(join(tmp, "s.wasm"))));
    for (const o of fx.objects)
        if (!seen.has(o)) {
            seen.add(o);
            standalone.push(add(`o${n++}_${basename(o)}`, readFileSync(o)));
        }
    for (const a of fx.archives)
        if (!seen.has(a)) {
            seen.add(a);
            corpus.push(add(basename(a), readFileSync(a)));
        }
}
{
    const fdir = join(tmp, "foreign");
    mkdirSync(fdir);
    for (const f of foreign(m, fdir))
        standalone.push(add(f, readFileSync(join(fdir, f))));
}
corpus.push(...standalone);
for (const f of readdirSync(m.sdk_libs).filter((n) => n.endsWith(".a")).sort())
    corpus.push(add(f, readFileSync(join(m.sdk_libs, f))));

// ------------------------------------------------------------ the code

let lines = 0, named = 0;
const MODES = [["-d"], ["-dr"], ["-d", "-C"], ["-dr", "-C"], ["-d", "--no-show-raw-insn"]];
for (const mode of MODES) {
    const raw = !mode.includes("--no-show-raw-insn");
    for (const files of [inputs, corpus]) {
        const want = llvm([...mode, ...files]);
        if (want.status !== 0 || want.err)
            die(`llvm-objdump ${mode.join(" ")}: status ${want.status}: ${want.err}`);
        const got = sh(`disasm ${mode.join(" ")} ${files.join(" ")}`);
        const what = `[${mode.join(" ")}]`;
        if (got.status !== 0 || got.err) {
            bad.push(`${what}: status ${got.status}: ${got.err.slice(0, 500)}`);
            continue;
        }
        const d = against(got.out, want.out, raw);
        if (d)
            bad.push(`${what}: ${d}`);
        flow(got.out, what);
        if (mode.join(" ") === "-dr")
            named += names(got.out, what);
        lines += want.out.split("\n").length;
    }
}
for (const f of Object.keys(m.fixtures).map((k) => `fx_${k}.wasm`)) {
    const got = sh(`disasm ${f}`);
    named += calls(got.out, f, imported(readFileSync(join(dir, f))));
}
if (!named)
    bad.push("no call was named");

// ------------------------------------------------------------ the data

// The segments of a module: [address, bytes], as the tool places them.
function segments(bytes) {
    const b = new Uint8Array(bytes);
    let p = 8;
    const u = () => {
        let v = 0, s = 0, x;
        do {
            x = b[p++];
            v += (x & 0x7f) * 2 ** s;
            s += 7;
        } while (x & 0x80);
        return v;
    };
    const segs = [];
    while (p < b.length) {
        const id = b[p++], size = u(), end = p + size;
        if (id === 11) {
            for (let k = u(); k--;) {
                const flags = u();
                let base = 0;
                if (flags === 2)
                    u();
                if (flags !== 1) {
                    const op = b[p++];
                    let v = 0, s = 0, x;
                    do {
                        x = b[p++];
                        v |= (x & 0x7f) << s;
                        s += 7;
                    } while (x & 0x80);
                    if (s < 32 && (x & 0x40))
                        v |= -1 << s;
                    base = (op === 0x41 || op === 0x42) && b[p] === 0x0b ? v >>> 0 : 0;
                    while (b[p++] !== 0x0b)
                        ;
                }
                const len = u();
                segs.push([base, b.subarray(p, p + len)]);
                p += len;
            }
        }
        p = end;
    }
    return segs;
}

// The rows of each DATA section in the tool's output, as bytes by address,
// one map per module.
function rows(out) {
    const mods = [];
    let cur = null, inData = false, at = 0;
    for (const line of out.split("\n")) {
        if (line.endsWith(":\tfile format wasm")) {
            cur = new Map();
            mods.push(cur);
            inData = false;
        } else if (line.startsWith("Disassembly of section ")) {
            inData = line === "Disassembly of section DATA:";
        } else if (inData && /^[0-9a-f]{8}(?:[0-9a-f]{8})? </.test(line)) {
            at = parseInt(line, 16);
        } else if (inData && line === "\t\t...") {
            cur.skip = true;
        } else if (inData && /^ *[0-9a-f]+:/.test(line)) {
            const a = line.slice(0, line.indexOf(":")), rest = line.slice(a.length + 1);
            const addr = parseInt(a, 16);
            if (!cur.skip && addr !== at)
                bad.push(`data row at ${a.trim()} follows ${at.toString(16)}`);
            const hex = rest.slice(0, 48).trim().split(" ");
            hex.forEach((h, i) => cur.set(addr + i, parseInt(h, 16)));
            const text = rest.slice(50);
            hex.forEach((h, i) => {
                const c = parseInt(h, 16);
                if (text[i] !== (c >= 0x20 && c < 0x7f ? String.fromCharCode(c) : "."))
                    bad.push(`data row at ${a.trim()}: text ${JSON.stringify(text)}`);
            });
            at = addr + hex.length;
            cur.skip = false;
        }
    }
    return mods;
}

let bytes = 0;
{
    const files = [...standalone, "locals.wasm"];
    const got = sh(`disasm -D ${files.join(" ")}`);
    const code = sh(`disasm ${files.join(" ")}`);
    if (got.status !== 0 || got.err)
        bad.push(`data: status ${got.status}: ${got.err.slice(0, 500)}`);
    // Without its DATA sections, the output is the default, -d.
    const bare = got.out.replace(/\nDisassembly of section DATA:\n(\n[^\n]*)*?\n(?=\n[^\n]*:\tfile format wasm\n|$)/g, "");
    if (bare !== code.out)
        bad.push(`data: without DATA, not -d's: ${differ(bare, code.out)}`);
    const mods = rows(got.out);
    files.forEach((f, i) => {
        const have = mods[i] ?? new Map();
        for (const [base, content] of segments(readFileSync(join(dir, f)))) {
            // A passive segment starts at 0, as does one at a global.
            for (let k = 0; k < content.length; k++) {
                const v = have.get(base + k) ?? 0;
                if (v !== content[k]) {
                    bad.push(`${f}: data at ${(base + k).toString(16)} is ${v}, not ${content[k]}`);
                    break;
                }
            }
            bytes += content.length;
        }
    });
}

// An object, whole: its code, its relocations, its data and theirs.
{
    const o = add("golden.o", readFileSync(m.fixtures.fnptr.objects[1]));
    const got = sh(`disasm -Dr ${o}`);
    const golden = join(HERE, "golden.txt");
    if (process.env.BLESS)
        writeFileSync(golden, got.out);
    else if (got.status !== 0 || got.out !== readFileSync(golden, "utf8"))
        bad.push(`golden: status ${got.status}: ${got.err} ${differ(got.out, readFileSync(golden, "utf8"))}`);
}

// ------------------------------------------------------------ errors

const ar_dir = join(tmp, "ar");
mkdirSync(ar_dir);
writeFileSync(join(ar_dir, "tr.o"), new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 1, 5, 0]));
writeFileSync(join(ar_dir, "notes.txt"), "not an object\n");
copyFileSync(m.fixtures.hello.objects[0], join(ar_dir, "hello.o"));
execFileSync(m.ar, ["rcS", "--format=gnu", "bad.a", "tr.o", "notes.txt", "hello.o"], { cwd: ar_dir });
add("bad.a", readFileSync(join(ar_dir, "bad.a")));
add("notwasm", "hello world\n");
add("trunc", new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 1, 5, 0]));
add("bc", new Uint8Array([0x42, 0x43, 0xc0, 0xde, 1, 2, 3, 4]));
add("h.o", readFileSync(m.fixtures.hello.objects[0]));

const e = (s) => `disasm: error: ${s}\n`;
const ERRORS = [
    ["disasm -d bad.a", e("bad.a(tr.o): section at file offset 0x8: runs past the end of the file")],
    ["disasm -d h.o nosuch h.o", e("cannot open nosuch: not found")],
    ["disasm notwasm", e("notwasm: not a wasm module")],
    ["disasm trunc", e("trunc: section at file offset 0x8: runs past the end of the file")],
    ["disasm bc", e("bc: LLVM bitcode (from -flto), not a wasm module")],
    ["disasm --foo h.o", e("unknown argument '--foo'")],
    ["disasm -dx h.o", e("unknown argument '-x'")],
];
for (const [cmd, err] of ERRORS) {
    const got = sh(cmd);
    if (got.status !== 1 || got.err !== err)
        bad.push(`${cmd}: status ${got.status}: ${JSON.stringify(got.err)}, expected ${JSON.stringify(err)}`);
}
// What is printed beside an error is still the good modules.
{
    const got = sh("disasm -d h.o nosuch h.o");
    const want = llvm(["-d", "h.o", "h.o"]);
    const d = against(got.out, want.out, true);
    if (d)
        bad.push(`beside an error: ${d}`);
    const ar = sh("disasm -d bad.a");
    const one = llvm(["-d", "h.o"]).out.replace("h.o:", "bad.a(hello.o):");
    const e = against(ar.out, one, true);
    if (e)
        bad.push(`bad.a: ${e}`);
}

// Standard input.
{
    const prog = readFileSync(m.fixtures.hello.reference);
    plant(H, "/tmp/in", new Uint8Array(prog));
    const want = llvm(["-d", "fx_hello.wasm"]).out;
    const got = sh("disasm -d - </tmp/in");
    const d = against(got.out, want.replace("fx_hello.wasm:", "<stdin>:"), true);
    if (got.status !== 0 || d)
        bad.push(`stdin: ${got.status} ${got.err} ${d}`);
}

// No file: the usage, bare or asked for; with an option, an error.
for (const cmd of ["disasm", "disasm -h", "disasm --help"]) {
    const r = sh(cmd);
    if (r.status !== 0 || r.err || !r.out.startsWith("Usage:\n    disasm "))
        bad.push(`${cmd}: status ${r.status}: ${r.out}${r.err}`);
}
{
    const r = sh("disasm -d");
    if (r.status !== 1 || r.err !== "disasm: error: no input file specified\n")
        bad.push(`disasm -d: status ${r.status}: ${JSON.stringify(r.err)}`);
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 40).join("\n  "));
console.log(`disasm ok: ${inputs.length + corpus.length} files in ${MODES.length} modes against ` +
            `llvm-objdump (${lines} lines), ${named} names, ${bytes} bytes of data, ${ERRORS.length} errors`);
