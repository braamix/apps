// as's objects linked with clang's. test/link/defines.s defines functions
// C calls, calls.s calls C and keeps a global of its own, and data.s
// has data C reads and writes, by the annotations of README.md's Data; link.c is
// the C half. as assembles both on Braam, ld and wasm-ld link them with the C
// object, the fixtures' driver and the SDK, the two outputs must be equal
// byte for byte, and the program must run and print what it should.

import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { boot, execute, FLAGS, get, linkers, manifest, plant, run } from "../../ld/test/wasmlib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const BUILD = join(HERE, "../../../../build/devel/wasm");
const AS = join(BUILD, "as/as.wasm");
const C = JSON.parse(readFileSync(join(BUILD, "as/test/link/link.json"), "utf8"));
const WAT = ["defines", "calls", "data"];
const WANT = "fib(10) = 55\nsum = 7\n421422\nhello from wat\n5 451 41 410\n";

function die(msg) {
    console.error("link: " + msg);
    process.exit(1);
}

const m = manifest();
const H = await boot();
const L = linkers(H, m);
const { wasm_ld, ld, inputs } = L;
const bad = [];

// as, on Braam.
plant(H, "/bin/as", new Uint8Array(readFileSync(AS)));
for (const w of WAT)
    plant(H, `/tmp/${w}.s`, readFileSync(join(HERE, `link/${w}.s`)));
run(H, `cd /tmp; as ${WAT.map((w) => w + ".s").join(" ")} 2>e`);
if (get(H, "/tmp/e"))
    die(`as: ${get(H, "/tmp/e")}`);
const tmp = mkdtempSync(join(tmpdir(), "as-link-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const objects = WAT.map((w) => {
    const b = H.store.files.get(`/tmp/${w}.o`);
    if (!b)
        die(`as wrote no ${w}.o`);
    writeFileSync(join(tmp, `${w}.o`), b);
    return join(tmp, `${w}.o`);
});

// Both linkers, with the memory defined rather than imported, so that
// neither needs __wasm_init_memory and the two can be equal: byte for byte,
// but for ld's braam section, which is last.
const all = inputs({ objects: [...objects, ...C.objects], libs: C.libs });
const base = [...FLAGS.filter((f) => f !== "--import-memory"), ...all];
const w = wasm_ld(base);
if (w.status !== 0)
    die(`wasm-ld: ${w.err}`);
const theirs = readFileSync(join(L.tmp, "out.wasm"));
H.store.files.delete("/tmp/out.wasm");
const got = ld(["--no-import-memory", ...base, "-o", "out.wasm"]);
if (got.status !== 0)
    die(`ld: ${got.err}`);
const tail = Buffer.from([0, 26, 5, ...Buffer.from("braam")]);
const ours = Buffer.from(H.store.files.get("/tmp/out.wasm"));
const at = ours.length - 28;
if (at < 0 || !ours.subarray(at, at + 8).equals(tail) || !ours.subarray(0, at).equals(theirs))
    bad.push("ld's module is not wasm-ld's");

// The program, as braam_add_program links it, run.
H.store.files.delete("/tmp/p");
const prog = ld([...FLAGS, ...all, "-o", "p"]);
if (prog.status !== 0)
    die(`ld: ${prog.err}`);
const r = execute(H, H.store.files.get("/tmp/p"));
if (r.status !== 0 || r.out !== WANT)
    bad.push(`the program: status ${r.status}, printed ${JSON.stringify(r.out)}, not ${JSON.stringify(WANT)}`);

if (bad.length)
    die(bad.join("\n  "));
console.log(`link ok: ${WAT.length} WAT objects linked with C by ld and wasm-ld alike, and run`);
