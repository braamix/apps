// The test suite run: every .wast script's commands, in order, with `as`
// assembling each text module and V8 running it. A module must assemble and
// instantiate; assert_malformed must be refused by `as` (or by V8, for a
// binary one), assert_invalid by `as` or V8, assert_unlinkable and
// assert_uninstantiable by V8 at instantiation. Every assert_return,
// assert_trap, assert_exhaustion and assert_exception must hold.
//
// JS cannot carry a v128, nor a signalling NaN, which it quiets: so a
// function whose type is all numbers is called from a module of its own,
// which imports it with that type and compares the results' bits there. One
// such module serves every assertion between two modules of a script, and
// `as` assembles it too. Functions with references in their type are
// called from JS.

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";
import { module as moduleForm, read } from "./wast.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");
const SUITE = join(HERE, "suite/core");

// Script:line, and what V8 does not implement.
const V8_LIMITS = {
    "memory64/memory64.wast:9": "a maximum of 2**48 pages: V8's limit is 2**18",
};

function die(msg) {
    console.error("spec: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
const as = await assembler(AS);
const bad = [];
const utf8 = (b) => new TextDecoder().decode(b);

// ------------------------------------------------------------ assembling

// `as --module` of many sources: bytes, or the message it refused with.
function assemble(sources) {
    const out = [];
    for (let at = 0; at < sources.length; at += 500) {
        const batch = sources.slice(at, at + 500);
        const inputs = Object.fromEntries(batch.map((s, k) => [`m${k}.wat`, s]));
        const r = as.run(["--module", ...Object.keys(inputs)], inputs);
        const said = new Map();
        for (const l of r.err.split("\n").filter((l) => l)) {
            const m = /^(m\d+)\.wat:(.*)$/.exec(l);
            if (!m)
                die(`as said: ${l}`);
            said.set(m[1], m[2]);
        }
        batch.forEach((_, k) => {
            const b = r.files[`m${k}.wasm`];
            out.push(b ? { bytes: b } : { error: said.get(`m${k}`) ?? "nothing written" });
        });
    }
    return out;
}

// ------------------------------------------------------------ a module's exports

// The type of each exported function: { params, results }, each a list of
// value types as WAT spells them, but "ref" for a concrete reference; and
// null for one whose type is not `(type (func …))`, which only JS can call.
function exportTypes(bytes) {
    let i = 8;
    const u8 = () => bytes[i++];
    const uleb = () => {
        let v = 0, s = 0, b;
        do {
            b = bytes[i++];
            v += (b & 0x7f) * 2 ** s;
            s += 7;
        } while (b & 0x80);
        return v;
    };
    const NUM = { 0x7f: "i32", 0x7e: "i64", 0x7d: "f32", 0x7c: "f64", 0x7b: "v128" };
    const HEAP = { 0x6e: "any", 0x6d: "eq", 0x6c: "i31", 0x6b: "struct", 0x6a: "array", 0x71: "none",
                   0x70: "func", 0x73: "nofunc", 0x69: "exn", 0x74: "noexn", 0x6f: "extern",
                   0x72: "noextern" };
    const valtype = () => {
        const b = u8();
        if (b === 0x63 || b === 0x64) {
            const h = bytes[i];
            uleb();
            return HEAP[h] ? `(ref ${b === 0x63 ? "null " : ""}${HEAP[h]})` : "ref";
        }
        return NUM[b] ?? (HEAP[b] ? `(ref null ${HEAP[b]})` : "ref");
    };
    const vec = (f) => Array.from({ length: uleb() }, f);
    const storage = () => (bytes[i] === 0x78 || bytes[i] === 0x77 ? u8() : valtype());
    const comp = (form) => {
        if (form === 0x60)
            return { params: vec(valtype), results: vec(valtype) };
        if (form === 0x5f)
            vec(() => (storage(), u8()));
        else
            storage(), u8();
        return null;
    };
    const sub = (form, alone) => {
        let plain = alone;
        if (form === 0x50 || form === 0x4f) {
            plain = plain && form === 0x4f && vec(uleb).length === 0;
            form = u8();
        }
        const c = comp(form);
        return plain ? c : null;
    };
    const limits = (memory) => {
        const flags = uleb();
        uleb();
        if (flags & 1)
            uleb();
        if (memory && flags & 8)
            uleb();
    };
    const name = () => {
        const n = uleb();
        i += n;
        return utf8(bytes.subarray(i - n, i));
    };
    const types = [], funcs = [], out = new Map();
    while (i < bytes.length) {
        const id = u8(), size = uleb(), end = i + size;
        if (id === 1) {
            vec(() => {
                const form = u8();
                if (form === 0x4e) {
                    const n = uleb();
                    for (let k = 0; k < n; k++)
                        types.push(sub(u8(), n === 1));
                } else {
                    types.push(sub(form, true));
                }
            });
        } else if (id === 2) {
            vec(() => {
                name(), name();
                const kind = u8();
                if (kind === 0)
                    funcs.push(types[uleb()]);
                else if (kind === 1)
                    valtype(), limits(false);
                else if (kind === 2)
                    limits(true);
                else if (kind === 3)
                    valtype(), u8();
                else
                    u8(), uleb();
            });
        } else if (id === 3) {
            vec(() => funcs.push(types[uleb()]));
        } else if (id === 7) {
            vec(() => {
                const n = name(), kind = u8(), x = uleb();
                if (kind === 0)
                    out.set(n, funcs[x]);
            });
        }
        i = end;
    }
    return out;
}

// ------------------------------------------------------------ literals, in JS

function int(text, bits) {
    let t = text.replace(/_/g, ""), neg = false;
    if (t[0] === "-" || t[0] === "+")
        neg = t[0] === "-", t = t.slice(1);
    let v = BigInt(t);
    if (neg)
        v = -v;
    return BigInt.asIntN(bits, v);
}

function float(text) {
    let t = text.replace(/_/g, ""), sign = 1;
    if (t[0] === "-" || t[0] === "+")
        sign = t[0] === "-" ? -1 : 1, t = t.slice(1);
    if (t.startsWith("nan"))
        return NaN;
    if (t === "inf")
        return sign * Infinity;
    const h = /^0x([0-9a-f]*)(?:\.([0-9a-f]*))?(?:p([-+]?\d+))?$/i.exec(t);
    if (!h)
        return sign * Number(t);
    const frac = h[2] ?? "";
    const m = BigInt("0x0" + h[1] + frac);
    return sign * Number(m) * 2 ** (Number(h[3] ?? 0) - 4 * frac.length);
}

// ------------------------------------------------------------ the wrappers

const quote = (b) => '"' + [...b].map((c) => "\\" + c.toString(16).padStart(2, "0")).join("") + '"';

// A check, in wasm, that local `r` of type `t` matches pattern `p`: an i32
// of 1 when it does.
function check(src, p, r) {
    const head = p.items[0].atom;
    if (head === "either")
        return p.items.slice(1).map((q) => check(src, q, r)).reduce((a, b) => `(i32.or ${a} ${b})`);
    const lit = p.items[1]?.atom;
    const text = utf8(src.subarray(p.start, p.end));
    const get = `(local.get ${r})`;
    if (head === "ref.null")
        return `(ref.is_null ${get})`;
    if (head === "ref.func")
        return `(i32.eqz (ref.is_null ${get}))`;
    const float = (bits, v, pat) => {
        const i = `i${bits}`;
        const b = `(${i}.reinterpret_f${bits} ${v})`;
        if (pat === "nan:canonical")
            return `(${i}.eq (${i}.and ${b} (${i}.const 0x${bits === 32 ? "7fffffff" : "7fffffffffffffff"})) ` +
                   `(${i}.const 0x${bits === 32 ? "7fc00000" : "7ff8000000000000"}))`;
        if (pat === "nan:arithmetic") {
            const q = bits === 32 ? "0x7fc00000" : "0x7ff8000000000000";
            return `(${i}.eq (${i}.and ${b} (${i}.const ${q})) (${i}.const ${q}))`;
        }
        return `(${i}.eq ${b} (${i}.reinterpret_f${bits} (f${bits}.const ${pat})))`;
    };
    switch (head) {
    case "i32.const":
        return `(i32.eq ${get} ${text})`;
    case "i64.const":
        return `(i64.eq ${get} ${text})`;
    case "f32.const":
        return float(32, get, lit);
    case "f64.const":
        return float(64, get, lit);
    case "v128.const": {
        const shape = lit, lanes = p.items.slice(2).map((x) => x.atom);
        if (!lanes.some((l) => l.startsWith("nan:")))
            return `(i8x16.all_true (i8x16.eq ${get} ${text}))`;
        const bits = shape === "f32x4" ? 32 : 64;
        return lanes.map((l, k) => float(bits, `(${shape}.extract_lane ${k} ${get})`, l))
            .reduce((a, b) => `(i32.and ${a} ${b})`);
    }
    }
    return null;
}

// ------------------------------------------------------------ JS values

const hostrefs = new Map();
const hostref = (n) => {
    if (!hostrefs.has(n))
        hostrefs.set(n, { hostref: n });
    return hostrefs.get(n);
};

function jsArg(p) {
    const head = p.items[0].atom, lit = p.items[1]?.atom;
    switch (head) {
    case "i32.const":
        return Number(int(lit, 32));
    case "i64.const":
        return int(lit, 64);
    case "f32.const":
    case "f64.const":
        return float(lit);
    case "ref.null":
        return null;
    case "ref.extern":
    case "ref.host":
        return hostref(Number(lit));
    }
    throw new Error(`an argument JS cannot give: ${head}`);
}

function jsMatch(p, v) {
    const head = p.items[0].atom, lit = p.items[1]?.atom;
    switch (head) {
    case "either":
        return p.items.slice(1).some((q) => jsMatch(q, v));
    case "i32.const":
        return (v | 0) === Number(int(lit, 32));
    case "i64.const":
        return BigInt.asIntN(64, v) === int(lit, 64);
    case "f32.const":
    case "f64.const": {
        const want = float(lit);
        const f = head === "f32.const" ? Math.fround : (x) => x;
        return Number.isNaN(want) ? Number.isNaN(v) : Object.is(f(v), f(want));
    }
    case "ref.null":
        return v === null;
    case "ref.extern":
    case "ref.host":
        return lit === undefined ? v !== null : v === hostref(Number(lit));
    case "ref.func":
        return typeof v === "function";
    case "ref.i31":
        return typeof v === "number";
    case "ref.any":
    case "ref.eq":
    case "ref.struct":
    case "ref.array":
        return v !== null && v !== undefined;
    }
    return false;
}

// ------------------------------------------------------------ the host

function spectest() {
    const g = (value, v) => new WebAssembly.Global({ value }, v);
    const print = () => {};
    return {
        print, print_i32: print, print_i64: print, print_f32: print, print_f64: print,
        print_i32_f32: print, print_f64_f64: print,
        global_i32: g("i32", 666), global_i64: g("i64", 666n),
        global_f32: g("f32", 666.6), global_f64: g("f64", 666.6),
        table: new WebAssembly.Table({ initial: 10, maximum: 20, element: "anyfunc" }),
        table64: new WebAssembly.Table({ initial: 10n, maximum: 20n, element: "anyfunc", address: "i64" }),
        memory: new WebAssembly.Memory({ initial: 1, maximum: 2 }),
        shared_memory: new WebAssembly.Memory({ initial: 1, maximum: 2, shared: true }),
    };
}

// ------------------------------------------------------------ planning

const scripts = readdirSync(SUITE, { recursive: true }).filter((f) => f.endsWith(".wast")).sort();

// Every script as steps; texts to assemble go into `texts`.
const texts = [];
const plans = scripts.map((f) => {
    const src = readFileSync(join(SUITE, f));
    let cmds = read(src);
    const heads = new Set(["module", "register", "invoke", "get"]);
    if (!cmds.some((c) => c.list && (heads.has(c.items[0]?.atom) || c.items[0]?.atom?.startsWith("assert_"))))
        cmds = [{ list: true, start: 0, end: src.length, items: [{ atom: "module" }], whole: true }];
    const text = (bytes) => texts.push(bytes) - 1;
    let line = 1, seen = 0;
    const lineOf = (at) => {
        for (; seen < at; seen++)
            line += src[seen] === 0x0a;
        return line;
    };
    const form = (n) => {
        if (n.whole)
            return { kind: "text", text: text(src) };
        const m = moduleForm(src, n);
        if (!m)
            return null;
        const k = n.items.findIndex((x, j) => j > 0 && x.atom !== "definition");
        const id = n.items[k]?.atom?.startsWith("$") ? n.items[k].atom : null;
        const def = n.items[1]?.atom === "definition";
        return m.kind === "binary" ? { kind: "binary", bytes: m.bytes, id, def }
                                   : { kind: "text", text: text(m.bytes), id, def };
    };
    const steps = cmds.filter((c) => c.list).map((c) => {
        const head = c.items[0]?.atom;
        const step = { head, node: c, line: lineOf(c.start) };
        if (head === "module" && c.items[1]?.atom === "instance")
            Object.assign(step, { head: "instance", id: c.items[2]?.atom, of: c.items[3]?.atom });
        else if (head === "module")
            step.module = form(c);
        else if (head?.startsWith("assert_") && c.items[1]?.items?.[0]?.atom === "module")
            step.module = form(c.items[1]);
        return step;
    });
    return { script: f, src, steps };
});

const built = assemble(texts);

// The bytes of a module form: from `as`, or the script's.
const bytesOf = (m) => (m.kind === "binary" ? { bytes: m.bytes } : built[m.text]);

// An action: its instance (by id, or the current), its name, its node.
function action(n) {
    const k = n.items[1]?.atom?.startsWith("$") ? 2 : 1;
    return { kind: n.items[0].atom, id: k === 2 ? n.items[1].atom : null, name: n.items[k].string,
             args: n.items.slice(k + 1) };
}

// Wrappers: per script, per stretch between two modules, the WAT of one
// module whose exported function c<k> runs one assertion.
const wrappers = [];
for (const plan of plans) {
    const slots = [], named = new Map(), defs = new Map();
    let current = -1, wrap = null;
    const flush = () => {
        if (wrap && wrap.funcs.length)
            wrap.text = wrappers.push(`(module\n${wrap.imports.join("\n")}\n${wrap.funcs.join("\n")})`) - 1;
        wrap = null;
    };
    for (const step of plan.steps) {
        const m = step.module;
        if (step.head === "module" || step.head === "instance") {
            flush();
            let bytes;
            if (step.head === "instance") {
                bytes = defs.get(step.of);
            } else if (m.def) {
                defs.set(m.id, bytesOf(m).bytes);
                continue;
            } else {
                bytes = bytesOf(m).bytes;
            }
            slots.push(bytes ? exportTypes(bytes) : new Map());
            current = slots.length - 1;
            step.slot = current;
            const id = step.head === "instance" ? step.id : m.id;
            if (id)
                named.set(id, current);
            continue;
        }
        if (step.head === "register") {
            const id = step.node.items[2]?.atom;
            step.slot = id ? named.get(id) : current;
            continue;
        }
        const n = step.head === "invoke" || step.head === "get" ? step.node : step.node.items[1];
        if (!n?.list || !["invoke", "get"].includes(n.items[0].atom))
            continue;
        const a = action(n);
        step.action = a;
        step.slot = a.id ? named.get(a.id) : current;
        const type = slots[step.slot]?.get(utf8(a.name));
        if (a.kind !== "invoke" || !type || [...type.params, ...type.results].includes("ref") ||
            a.args.some((x) => ["ref.extern", "ref.host"].includes(x.items[0].atom)))
            continue;
        const results = step.head === "assert_return" ? step.node.items.slice(2) : null;
        const checks = results?.map((p, k) => check(plan.src, p, `$r${k}`));
        if (checks?.includes(null) || (results && results.length !== type.results.length))
            continue;
        wrap ??= { imports: [], funcs: [], at: step };
        const f = wrap.funcs.length;
        wrap.imports.push(`(import "M${step.slot}" ${quote(a.name)} (func $f${f} ` +
                          `(param ${type.params.join(" ")}) (result ${type.results.join(" ")})))`);
        const args = a.args.map((x) => utf8(plan.src.subarray(x.start, x.end))).join(" ");
        const locals = type.results.map((t, k) => `(local $r${k} ${t})`).join(" ");
        const sets = type.results.map((_, k) => `(local.set $r${type.results.length - 1 - k})`).join(" ");
        const ok = checks?.length ? checks.reduce((x, y) => `(i32.and ${x} ${y})`) : "(i32.const 1)";
        wrap.funcs.push(`(func (export "c${f}") (result i32) ${locals}\n  (call $f${f} ${args}) ${sets}\n  ${ok})`);
        step.wrap = wrap;
        step.check = f;
    }
    flush();
}
const builtWrappers = assemble(wrappers);

// ------------------------------------------------------------ running

const count = {};
let limited = 0;
const tally = (what, ok, where, why) => {
    if (V8_LIMITS[where]) {
        limited++;
        if (ok)
            bad.push(`${where}: ${what}: passes, but is listed as beyond V8`);
        return;
    }
    count[what] ??= [0, 0];
    count[what][ok ? 0 : 1]++;
    if (!ok)
        bad.push(`${where}: ${what}: ${why}`);
};

function compile(bytes) {
    return new WebAssembly.Module(bytes);
}

for (const plan of plans) {
    const registry = { spectest: spectest() };
    const slots = [], named = new Map(), defs = new Map();
    const wrapped = new Map(); // wrapper → its instance, or the error
    let current = -1;
    const imports = () => registry;

    // Runs an action; what it returned, or throws.
    const run = (step) => {
        const a = step.action;
        const inst = slots[step.slot];
        if (!inst)
            throw new Error(`no instance ${a.id ?? ""}`);
        if (step.wrap) {
            const w = step.wrap;
            if (!wrapped.has(w)) {
                const b = builtWrappers[w.text];
                try {
                    if (!b.bytes)
                        throw new Error(`as refused the wrapper: ${b.error}`);
                    const im = {};
                    for (const [k, s] of slots.entries())
                        if (s)
                            im[`M${k}`] = s.exports;
                    wrapped.set(w, new WebAssembly.Instance(compile(b.bytes), im));
                } catch (e) {
                    wrapped.set(w, e);
                }
            }
            const wi = wrapped.get(w);
            if (wi instanceof Error)
                throw new Error(`wrapper: ${wi.message}`);
            return { wrapped: wi.exports[`c${step.check}`]() };
        }
        const x = inst.exports[utf8(a.name)];
        if (a.kind === "get")
            return { value: x.value };
        return { value: x(...a.args.map(jsArg)) };
    };

    for (const step of plan.steps) {
        const where = `${plan.script}:${step.line}`;
        const m = step.module;
        try {
            switch (step.head) {
            case "module": {
                if (m.def) {
                    defs.set(m.id, bytesOf(m));
                    break;
                }
                const b = bytesOf(m);
                let inst = null, why = b.error;
                if (b.bytes) {
                    try {
                        inst = new WebAssembly.Instance(compile(b.bytes), imports());
                    } catch (e) {
                        why = e.message;
                    }
                }
                slots[step.slot] = inst;
                current = step.slot;
                if (m.id)
                    named.set(m.id, step.slot);
                tally("module", inst !== null, where, why);
                break;
            }
            case "instance": {
                const b = defs.get(step.of);
                let inst = null, why = "no definition";
                try {
                    inst = new WebAssembly.Instance(compile(b.bytes), imports());
                } catch (e) {
                    why = e.message;
                }
                slots[step.slot] = inst;
                current = step.slot;
                tally("module", inst !== null, where, why);
                break;
            }
            case "register": {
                const inst = slots[step.slot];
                if (inst)
                    registry[utf8(step.node.items[1].string)] = inst.exports;
                tally("register", !!inst, where, "no instance");
                break;
            }
            case "invoke":
            case "get":
                run(step);
                tally("action", true, where);
                break;
            case "assert_return": {
                const r = run(step);
                const want = step.node.items.slice(2);
                let ok;
                if ("wrapped" in r) {
                    ok = r.wrapped === 1;
                } else {
                    const got = want.length === 1 ? [r.value] : r.value ?? [];
                    ok = want.length === got.length && want.every((p, k) => jsMatch(p, got[k]));
                }
                tally("assert_return", ok, where, "wrong result");
                break;
            }
            case "assert_trap":
            case "assert_exhaustion":
            case "assert_exception": {
                const Want = { assert_trap: WebAssembly.RuntimeError, assert_exhaustion: RangeError,
                               assert_exception: WebAssembly.Exception }[step.head];
                let thrown = null;
                if (m) {
                    const b = bytesOf(m);
                    try {
                        new WebAssembly.Instance(compile(b.bytes), imports());
                    } catch (e) {
                        thrown = e;
                    }
                } else {
                    try {
                        run(step);
                    } catch (e) {
                        thrown = e;
                    }
                }
                tally(m ? "assert_uninstantiable" : step.head, thrown instanceof Want, where,
                      thrown ? thrown.message : "returned");
                break;
            }
            case "assert_malformed":
            case "assert_invalid":
            case "assert_unlinkable":
            case "assert_uninstantiable": {
                const b = bytesOf(m);
                let stage = "as", why = b.error;
                if (b.bytes) {
                    stage = "compile";
                    try {
                        const mod = compile(b.bytes);
                        stage = "link";
                        new WebAssembly.Instance(mod, imports());
                        stage = "ran";
                    } catch (e) {
                        why = e.message;
                        if (stage === "link" && e instanceof WebAssembly.RuntimeError)
                            stage = "start";
                    }
                }
                const want = { assert_malformed: m.kind === "binary" ? ["compile"] : ["as"],
                               assert_invalid: ["as", "compile"], assert_unlinkable: ["link"],
                               assert_uninstantiable: ["start"] }[step.head];
                tally(step.head, want.includes(stage), where,
                      stage === "ran" ? "accepted" : `refused by ${stage}: ${why}`);
                break;
            }
            default:
                tally("unknown command", false, where, step.head);
            }
        } catch (e) {
            tally(step.head, false, where, e.message);
        }
    }
}

const total = Object.entries(count).map(([k, [ok, no]]) => `${k} ${ok}${no ? `/${ok + no}` : ""}`);
if (bad.length)
    die(`${bad.length} failures (${total.join(", ")}):\n  ` + bad.slice(0, 60).join("\n  "));
console.log(`spec ok: ${scripts.length} scripts: ${total.join(", ")}; ${limited} beyond V8`);
