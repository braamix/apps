// Writing. Every fixture is linked by ld on Braam, and the module must be
// valid, have the surface a Braam program has, and run as wasm-ld's build
// runs. Section by section it must be wasm-ld's own without --import-memory,
// which writes active segments as ld does: TYPE, FUNCTION, TABLE, GLOBAL,
// ELEM, CODE, DATA, name, producers and target_features byte for byte, and
// the same imports and exports but the memory. That holds at -O1, where
// strings are merged, at -O0, where they are not, and with
// --compress-relocations, which runs as well. The data fixture runs twice, so .bss is seen to start zeroed after
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

const IDS_BY_NAME = Object.fromEntries(Object.values(IDS).map((n) => [n, true]));

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

// Its warnings, which wasm-ld's must equal.
let warned = "";

function link(name, fx, level = ["-O1"]) {
    H.store.files.delete("/tmp/out.wasm");
    const got = ld([...FLAGS, ...level, ...inputs(fx), "-o", "out.wasm"]);
    warned = got.err;
    if (got.status !== 0 || got.err.includes("error:")) {
        bad.push(`${name} ${level.join(" ")}: ld fails: ${got.err}`);
        return null;
    }
    return H.store.files.get("/tmp/out.wasm");
}

let compared = 0;

// Whether the link with `level`'s flags is wasm-ld's, section by section.
function compare(name, fx, level) {
    const what = `${name} ${level.join(" ")}`;
    const bytes = link(name, fx, level);
    if (!bytes)
        return null;
    if (!WebAssembly.validate(bytes)) {
        bad.push(`${what}: not valid wasm`);
        return null;
    }
    const args = [...FLAGS.filter((f) => f !== "--import-memory"), ...level, ...inputs(fx)];
    const want = wasm_ld(args);
    if (want.status !== 0)
        die(`wasm-ld fails on ${what}: ${want.err}`);
    if (warned !== want.err.replaceAll("wasm-ld: ", "ld: "))
        bad.push(`${what}: stderr ${JSON.stringify(warned)}, wasm-ld's ${JSON.stringify(want.err)}`);
    const ref = new Uint8Array(readFileSync(join(tmp, "out.wasm")));
    const ours = by_name(bytes), theirs = by_name(ref);
    const before = bad.length;
    // Copied custom sections, debug info among them, are compared as well.
    const copied = [...theirs.keys()].filter((s) => !(s in IDS_BY_NAME) && !EXACT.includes(s));
    for (const s of [...EXACT, ...copied]) {
        const a = ours.get(s), b = theirs.get(s);
        if (!a && !b)
            continue;
        if (!a || !b || !a.equals(b)) {
            let at = 0;
            while (a && b && at < a.length && a[at] === b[at])
                at++;
            bad.push(`${what}: ${s} differs at +0x${at.toString(16)} ` +
                     `(ld ${a?.length ?? "none"} bytes, wasm-ld ${b?.length ?? "none"})`);
        } else {
            compared += a.length;
        }
    }
    if (surface(bytes) !== surface(ref))
        bad.push(`${what}: surface\n  ld:      ${surface(bytes)}\n  wasm-ld: ${surface(ref)}`);
    const extra = [...ours.keys()].filter((s) => !EXACT.includes(s) && !copied.includes(s) &&
        !["IMPORT", "EXPORT", "braam"].includes(s));
    if (extra.length)
        bad.push(`${what}: sections wasm-ld's has not: ${extra.join(" ")}`);
    return bad.length === before ? bytes : null;
}

for (const [name, fx] of Object.entries(m.fixtures)) {
    compare(name, fx, ["-O0"]);
    // Run only what matched: a mislinked program can spin, and the harness
    // has no timeout.
    const packed = compare(name, fx, ["--compress-relocations", "--strip-debug"]);
    if (packed)
        for (const b of check_run(H, name, packed))
            bad.push(`${name} packed: ${b}`);
    const bytes = compare(name, fx, ["-O1"]);
    if (bytes)
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
ok(`${Object.keys(m.fixtures).length} fixtures link on Braam and run; at -O0, -O1 and packed, ` +
   `${compared} bytes of sections equal wasm-ld's`);
