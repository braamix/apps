// The front end. With no flags, every fixture links as braam_add_program
// links it: the same bytes as --braam and as wasm-ld's spelling of that set,
// and a program that runs. Then -L and -l from a command line, with a.out as
// the output; a missing library, refused in wasm-ld's words; and a failed
// link, which leaves no output behind.

import { boot, check_run, check_surface, die, FLAGS, get, linkers, manifest, ok, plant, run }
    from "./wasmlib.mjs";

const m = manifest();
const H = await boot();
const { inputs, wasm_ld, ld } = linkers(H, m);

const bad = [];
const SET = ["--no-entry", "--gc-sections", "--stack-first", "-z", "stack-size=131072",
             "--import-memory"];

function same(what, a, b) {
    if (a !== b)
        bad.push(`${what}:\n  ld:      ${JSON.stringify(a)}\n  wasm-ld: ${JSON.stringify(b)}`);
}

// Links into /tmp/<to>; its bytes, or null.
function link(name, args, to = "p") {
    H.store.files.delete(`/tmp/${to}`);
    const got = ld([...args, "-o", to]);
    if (got.status !== 0 || got.err) {
        bad.push(`${name}: ld ${args.slice(0, 2).join(" ")}... fails: ${got.err}`);
        return null;
    }
    return Buffer.from(H.store.files.get(`/tmp/${to}`));
}

for (const [name, fx] of Object.entries(m.fixtures)) {
    const plain = link(name, inputs(fx));
    const braam = link(name, ["--braam", ...inputs(fx)]);
    const spelt = link(name, [...SET, ...inputs(fx)]);
    if (!plain || !braam || !spelt)
        continue;
    if (!plain.equals(braam))
        bad.push(`${name}: no flags and --braam differ`);
    if (!plain.equals(spelt))
        bad.push(`${name}: no flags and ${SET.join(" ")} differ`);
    const before = bad.length;
    for (const b of check_surface(name, plain))
        bad.push(`${name}: ${b}`);
    if (bad.length === before)
        for (const b of check_run(H, name, plain))
            bad.push(`${name}: ${b}`);
}

// -L and -l, typed rather than read from a file, and no -o. The module is
// named after the output, so the reference is written to a.out too.
const hello = m.fixtures.hello;
const objs = inputs(hello).slice(0, hello.objects.length);
const want = link("hello", inputs(hello), "a.out");
plant(H, "/tmp/l", "-L.\n-lbraam_proc\n-lbraam_ui\n");
for (const k of ["/tmp/a.out", "/tmp/e", "/tmp/s"])
    H.store.files.delete(k);
run(H, `cd /tmp; ld ${objs.join(" ")} @l 2>e; echo $? >s`);
same("-l: status", get(H, "/tmp/s"), "0\n");
same("-l: stderr", get(H, "/tmp/e"), "");
const aout = H.store.files.get("/tmp/a.out");
if (!aout || !want || !Buffer.from(aout).equals(want))
    bad.push("-l: a.out is not the link of the named archives");

// A missing library is refused before anything is linked.
const missing = [...FLAGS, ...objs, "-L.", "-lnope"];
H.store.files.delete("/tmp/p");
const got = ld([...missing, "-o", "p"]);
const ref = wasm_ld(missing);
same("-lnope: status", got.status, 1);
same("-lnope: stderr", got.err, ref.err.replaceAll("wasm-ld: ", "ld: "));
if (H.store.files.has("/tmp/p"))
    bad.push("-lnope: left an output file");

// A link that fails writes nothing.
for (const [name, fx] of Object.entries(m.bad)) {
    H.store.files.delete("/tmp/p");
    const r = ld([...inputs(fx), "-o", "p"]);
    if (r.status !== 1 || !r.err.startsWith("ld: error: "))
        bad.push(`${name}: status ${r.status}, stderr ${JSON.stringify(r.err)}`);
    if (H.store.files.has("/tmp/p"))
        bad.push(`${name}: left an output file`);
}

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`${Object.keys(m.fixtures).length} fixtures link and run with no flags; -l; ` +
   `${Object.keys(m.bad).length + 1} refusals leave no output`);
