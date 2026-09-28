// Symbol resolution. For every fixture, ld on Braam and wasm-ld on the
// host are given the same inputs and flags, and must load the same files
// in the same order (--trace) for the same reasons (--why-extract). Links that
// must fail must fail with wasm-ld's words. Then --dump-symtab is held to what
// the fixtures' sources say each name resolves to.

import { boot, die, FLAGS, linkers, manifest, ok } from "./wasmlib.mjs";

const m = manifest();

const H = await boot();
const { inputs, wasm_ld, ld } = linkers(H, m);

const bad = [];
const same = (what, a, b) => {
    if (a !== b)
        bad.push(`${what}:\n  ld:      ${JSON.stringify(a)}\n  wasm-ld: ${JSON.stringify(b)}`);
};

// ---------------------------------------------------------------- loading

let loaded = 0;
for (const [name, fx] of Object.entries(m.fixtures)) {
    const args = [...FLAGS, "--trace", "--why-extract=why", ...inputs(fx)];
    const want = wasm_ld(args);
    if (want.status !== 0)
        die(`wasm-ld fails on ${name}: ${want.err}`);
    const got = ld(args);
    same(`${name}: status`, got.status, 0);
    same(`${name}: stderr`, got.err, want.err.replaceAll("wasm-ld: ", "ld: "));
    same(`${name}: --trace`, got.out, want.out);
    same(`${name}: --why-extract`, got.why, want.why);
    loaded += want.out.split("\n").length - 1;
}

// ---------------------------------------------------------------- refusals

// wasm-ld's words, as ld says them.
const as_ld = (s) => s.replaceAll("wasm-ld: ", "ld: ");

function refuse(what, args) {
    const want = wasm_ld(args);
    const got = ld(args);
    same(`${what}: status`, got.status, 1);
    same(`${what}: stderr`, got.err, as_ld(want.err));
}

for (const [name, fx] of Object.entries(m.bad))
    refuse(name, [...FLAGS, ...inputs(fx)]);
refuse("error limit", [...FLAGS, "--error-limit=2", ...inputs(m.bad.bad_undef)]);
refuse("missing library", [...FLAGS, ...inputs(m.fixtures.hello), "-L.", "-lnothere"]);
refuse("--export", [...FLAGS, ...inputs(m.fixtures.hello), "--export=nothere"]);
refuse("--entry", [...FLAGS.filter((f) => f !== "--no-entry"),
    ...inputs(m.fixtures.hello), "--entry=nothere"]);

// ---------------------------------------------------------------- symtab

// name -> [state, kind, file, import], from --dump-symtab.
function symtab(fx, extra = []) {
    const got = ld([...FLAGS, "--dump-symtab", ...extra, ...inputs(fx)]);
    if (got.status !== 0)
        die(`--dump-symtab failed: ${got.err}`);
    const t = new Map();
    for (const line of got.out.split("\n").filter(Boolean)) {
        const [state, kind, name, file, imp] = line.split("\t");
        t.set(state === "comdat" ? `comdat ${kind}` : name,
              state === "comdat" ? ["comdat", "", name] : [state, kind, file, imp]);
    }
    return t;
}

function expect(fx, rows, extra) {
    const t = symtab(m.fixtures[fx] ?? m.bad[fx], extra);
    for (const [name, ...want] of rows) {
        const got = t.get(name);
        const w = want.join(" ").trim();
        const g = got ? got.filter((x) => x !== undefined).join(" ").trim() : "(absent)";
        if (g !== w)
            bad.push(`${fx}: ${name} is ${g}, expected ${w}`);
    }
}

expect("weak", [
    ["weak_who", "defined", "function", "weak_b.c.obj"],
    ["weak_only", "weak", "function", "weak_a.c.obj"],
    ["weak_value", "defined", "data", "weak_b.c.obj"],
]);
expect("undef", [
    ["absent_fn", "weak-undefined", "function", "undef_a.c.obj"],
    ["absent_var", "weak-undefined", "data", "undef_a.c.obj"],
    ["present_fn", "defined", "function", "undef_b.c.obj"],
    ["present_var", "defined", "data", "undef_b.c.obj"],
]);
expect("comdat", [
    ["_Z14comdat_counterv", "weak", "function", "comdat_a.cpp.obj"],
    ["comdat _Z14comdat_counterv", "comdat", "", "comdat_a.cpp.obj"],
    ["_ZZ14comdat_countervE5count", "weak", "data", "comdat_a.cpp.obj"],
]);
expect("archives", [
    ["one_a", "defined", "function", "libfxa_one.a(one_a.c.obj)"],
    ["two_b", "defined", "function", "libfxa_two.a(two_b.c.obj)"],
    ["one_c", "defined", "function", "libfxa_one.a(one_c.c.obj)"],
    ["one_unused", "lazy", "function", "libfxa_one.a(one_unused.c.obj)"],
]);
expect("imports", [
    ["kernel_sys", "import", "function", "imports.c.obj", "kernel.sys"],
    ["__stack_pointer", "synthetic", "global", "<internal>"],
    ["__wasm_call_ctors", "synthetic", "function", "<internal>"],
    ["__indirect_function_table", "synthetic", "table", "<internal>"],
]);
expect("bad_undef", [
    ["nowhere", "import", "function", "bad_undef_a.c.obj", "env.nowhere"],
    ["nowhere_var", "undefined", "data", "bad_undef_a.c.obj"],
], ["--allow-undefined"]);

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`${Object.keys(m.fixtures).length} fixtures load as wasm-ld loads them ` +
   `(${loaded} files); ${Object.keys(m.bad).length + 4} refusals in its words; symtab`);
