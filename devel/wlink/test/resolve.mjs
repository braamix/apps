// Step 2: symbol resolution. For every fixture, wlink on Braam and wasm-ld on
// the host are given the same inputs and flags, and must load the same files
// in the same order (--trace) for the same reasons (--why-extract). Links that
// must fail must fail with wasm-ld's words. Then --dump-symtab is held to what
// the fixtures' sources say each name resolves to.

import { execFileSync, spawnSync } from "node:child_process";
import { copyFileSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { boot, die, get, manifest, ok, plant, run } from "./wlinklib.mjs";

const m = manifest();
const WASM_LD = m.wasm_ld;

const FLAGS = ["--no-demangle", "--import-memory", "--initial-memory=1048576", "--no-entry",
               "--gc-sections", "--stack-first", "-z", "stack-size=131072"];

// Every input by its base name, in one directory on each side.
const tmp = mkdtempSync(join(tmpdir(), "wlink-resolve-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const H = await boot();
plant(H, "/bin/wlink", new Uint8Array(readFileSync(m.wlink)));
const where = new Map();
function place(path) {
    const b = basename(path);
    if (where.has(b) && where.get(b) !== path)
        die(`two inputs are named ${b}`);
    if (!where.has(b)) {
        where.set(b, path);
        copyFileSync(path, join(tmp, b));
        plant(H, `/tmp/${b}`, new Uint8Array(readFileSync(path)));
    }
    return b;
}

function wasm_ld(args) {
    const r = spawnSync(WASM_LD, [...args, "-o", "out.wasm"], { cwd: tmp, encoding: "utf8" });
    if (r.error)
        die(`cannot run ${WASM_LD}: ${r.error.message}`);
    let why = "";
    try {
        why = readFileSync(join(tmp, "why"), "utf8");
    } catch {}
    rmSync(join(tmp, "why"), { force: true });
    return { out: r.stdout, err: r.stderr, status: r.status, why };
}

function wlink(args) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s", "/tmp/why"])
        H.store.files.delete(k);
    plant(H, "/tmp/rsp", args.join("\n") + "\n");
    run(H, "cd /tmp; wlink @rsp >o 2>e; echo $? >s");
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "",
             status: Number(get(H, "/tmp/s")), why: get(H, "/tmp/why") ?? "" };
}

const bad = [];
const same = (what, a, b) => {
    if (a !== b)
        bad.push(`${what}:\n  wlink:   ${JSON.stringify(a)}\n  wasm-ld: ${JSON.stringify(b)}`);
};

function inputs(fx) {
    return [...fx.objects, ...(fx.archives ?? []), ...fx.libs].map(place);
}

// ---------------------------------------------------------------- loading

let loaded = 0;
for (const [name, fx] of Object.entries(m.fixtures)) {
    const args = [...FLAGS, "--trace", "--why-extract=why", ...inputs(fx)];
    const want = wasm_ld(args);
    if (want.status !== 0)
        die(`wasm-ld fails on ${name}: ${want.err}`);
    const got = wlink(args);
    same(`${name}: status`, got.status, 0);
    same(`${name}: stderr`, got.err, "");
    same(`${name}: --trace`, got.out, want.out);
    same(`${name}: --why-extract`, got.why, want.why);
    loaded += want.out.split("\n").length - 1;
}

// ---------------------------------------------------------------- refusals

// wasm-ld's words, as wlink says them. A signature mismatch is wasm-ld's
// warning and wlink's error.
const as_wlink = (s) => s.replaceAll("wasm-ld: ", "wlink: ").replaceAll("warning: ", "error: ");

function refuse(what, args) {
    const want = wasm_ld(args);
    const got = wlink(args);
    same(`${what}: status`, got.status, 1);
    same(`${what}: stderr`, got.err, as_wlink(want.err));
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
    const got = wlink([...FLAGS, "--dump-symtab", ...extra, ...inputs(fx)]);
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
