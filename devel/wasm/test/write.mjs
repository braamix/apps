// Writing. Every fixture is linked by ld on Braam, and the module must be
// valid, have the surface a Braam program has, and run as wasm-ld's build
// runs. Section by section it must be wasm-ld's own at -O0 without
// --import-memory, which writes active segments as ld does: TYPE,
// FUNCTION, TABLE, GLOBAL, ELEM, CODE, DATA, name, producers and
// target_features byte for byte, and the same imports and exports but the
// memory. The data fixture runs twice, so .bss is seen to start zeroed after
// a process has dirtied it.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { boot, check_run, check_surface, die, execute, FLAGS, linkers, manifest, ok, sections }
    from "./wasmlib.mjs";

const m = manifest();
const H = await boot();
const { tmp, inputs, wasm_ld, ld } = linkers(H, m);

const bad = [];
const EXACT = ["TYPE", "FUNCTION", "TABLE", "GLOBAL", "ELEM", "CODE", "DATA", "name", "producers",
               "target_features"];
const IDS = { 1: "TYPE", 2: "IMPORT", 3: "FUNCTION", 4: "TABLE", 5: "MEMORY", 6: "GLOBAL",
              7: "EXPORT", 8: "START", 9: "ELEM", 10: "CODE", 11: "DATA", 12: "DATACOUNT" };

function by_name(bytes) {
    const out = new Map();
    for (const s of sections(bytes))
        out.set(s.id ? IDS[s.id] : s.name, Buffer.from(s.body));
    return out;
}

function surface(bytes) {
    const mod = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(mod).filter((i) => i.kind !== "memory")
        .map((i) => `${i.module}.${i.name}:${i.kind}`);
    const exports = WebAssembly.Module.exports(mod).filter((e) => e.kind !== "memory")
        .map((e) => `${e.name}:${e.kind}`);
    return JSON.stringify({ imports, exports });
}

function link(name, fx) {
    H.store.files.delete("/tmp/out.wasm");
    const got = ld([...FLAGS, ...inputs(fx), "-o", "out.wasm"]);
    if (got.status !== 0 || got.err) {
        bad.push(`${name}: ld fails: ${got.err}`);
        return null;
    }
    return H.store.files.get("/tmp/out.wasm");
}

let compared = 0;
for (const [name, fx] of Object.entries(m.fixtures)) {
    const bytes = link(name, fx);
    if (!bytes)
        continue;
    if (!WebAssembly.validate(bytes)) {
        bad.push(`${name}: not valid wasm`);
        continue;
    }
    const args = [...FLAGS.filter((f) => f !== "--import-memory"), ...inputs(fx), "-O0"];
    const want = wasm_ld(args);
    if (want.status !== 0)
        die(`wasm-ld fails on ${name}: ${want.err}`);
    const ref = new Uint8Array(readFileSync(join(tmp, "out.wasm")));
    const ours = by_name(bytes), theirs = by_name(ref);
    const before = bad.length;
    for (const s of EXACT) {
        const a = ours.get(s), b = theirs.get(s);
        if (!a && !b)
            continue;
        if (!a || !b || !a.equals(b)) {
            let at = 0;
            while (a && b && at < a.length && a[at] === b[at])
                at++;
            bad.push(`${name}: ${s} differs at +0x${at.toString(16)} ` +
                     `(ld ${a?.length ?? "none"} bytes, wasm-ld ${b?.length ?? "none"})`);
        } else {
            compared += a.length;
        }
    }
    if (surface(bytes) !== surface(ref))
        bad.push(`${name}: surface\n  ld:      ${surface(bytes)}\n  wasm-ld: ${surface(ref)}`);
    const extra = [...ours.keys()].filter((s) => !EXACT.includes(s) &&
        !["IMPORT", "EXPORT", "braam"].includes(s));
    if (extra.length)
        bad.push(`${name}: sections wasm-ld's has not: ${extra.join(" ")}`);
    // Run only what matched: a mislinked program can spin, and the harness
    // has no timeout.
    if (bad.length === before)
        for (const b of [...check_surface(name, bytes), ...check_run(H, name, bytes)])
            bad.push(`${name}: ${b}`);
}

// .bss is not written: a second run must find it zeroed again.
const data = link("data", m.fixtures.data);
if (data && !bad.length) {
    const second = execute(H, data);
    if (!second.out.includes("bss zero\n"))
        bad.push(`data: second run: ${JSON.stringify(second.out)}`);
}

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`${Object.keys(m.fixtures).length} fixtures link on Braam and run; ` +
   `${compared} bytes of sections equal wasm-ld's`);
