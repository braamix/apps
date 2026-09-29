// The examples, built on Braam by their own build.sh with as, ar and ld,
// and run. Each program must have the surface of a Braam program, be
// what wasm-ld links from the same objects, and print what the golden
// transcript says (BLESS=1 writes it). cat must copy bytes exactly.

import { readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { boot, FLAGS, get, linkers, manifest, plant, run } from "../../ld/test/wasmlib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const SRC = join(HERE, "..");
const BUILD = join(HERE, "../../../../build/devel/wasm");
const GOLDEN = join(HERE, "examples.golden");
const PROGRAMS = ["hello", "echo", "cat", "fib", "primes"];
const LIBRARY = ["crt", "fmt", "args"];
const CASES = [
    "hello",
    "echo",
    "echo a bc 'd e' '' f",
    "echo 'héllo, мир'",
    "echo some text | cat",
    "cat </dev/null",
    "fib",
    "fib 10",
    "fib 0",
    "fib 93 | tail -n 2",
    "fib 94",
    "fib x",
    "fib 1 2",
    "primes",
    "primes 1000",
    "primes 2",
    "primes 1",
    "primes 10000001",
];

function die(msg) {
    console.error("examples: " + msg);
    process.exit(1);
}

const m = manifest();
const H = await boot();
const L = linkers(H, m);
for (const t of ["as", "ar"])
    plant(H, `/bin/${t}`, new Uint8Array(readFileSync(join(BUILD, `${t}/${t}.wasm`))));
const bad = [];

// A script, since a command line is at most sixty keys.
function sh(script) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
        H.store.files.delete(k);
    plant(H, "/tmp/c", `cd /tmp/ex\n{ ${script}; } >/tmp/o 2>/tmp/e\necho $? >/tmp/s\n`);
    run(H, "sh /tmp/c");
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "",
             status: Number(get(H, "/tmp/s")) };
}

// build.sh.
H.store.dirs.add("/tmp/ex");
const sources = readdirSync(SRC).filter((f) => f.endsWith(".s") || f === "build.sh");
for (const f of sources)
    plant(H, `/tmp/ex/${f}`, new Uint8Array(readFileSync(join(SRC, f))));
const b = sh("sh build.sh");
if (b.status !== 0 || b.err || b.out)
    die(`build.sh: status ${b.status}\n${b.out}${b.err}`);
const file = (f) => H.store.files.get(`/tmp/ex/${f}`) ?? die(`build.sh made no ${f}`);

// The surface of a Braam program.
const IMPORTS = "env.memory kernel.sys kernel.sys_async";
const EXPORTS = "_alloc _free _resume _sig _start";
for (const p of PROGRAMS) {
    const mod = new WebAssembly.Module(file(p));
    const im = WebAssembly.Module.imports(mod).map((i) => i.module + "." + i.name).sort().join(" ");
    const ex = WebAssembly.Module.exports(mod).map((e) => e.name).sort().join(" ");
    if (im !== IMPORTS)
        bad.push(`${p} imports ${im}`);
    if (ex !== EXPORTS)
        bad.push(`${p} exports ${ex}`);
}

// wasm-ld links the same objects to the same module, with the memory
// defined, as as/test/link.mjs explains, and but for ld's braam section.
writeFileSync(join(L.tmp, "libw.a"), file("libw.a"));
for (const o of [...PROGRAMS, ...LIBRARY])
    writeFileSync(join(L.tmp, `${o}.o`), file(`${o}.o`));
const tail = Buffer.from([0, 26, 5, ...Buffer.from("braam")]);
for (const p of PROGRAMS) {
    const lib = PROGRAMS.indexOf(p) < 3 ? [] : ["crt.o"];
    const inputs = [...lib, `${p}.o`, ...(lib.length ? ["libw.a"] : [])];
    const base = [...FLAGS.filter((f) => f !== "--import-memory"), ...inputs];
    for (const i of inputs)
        plant(H, `/tmp/${i}`, file(i));
    const w = L.wasm_ld(base);
    if (w.status !== 0)
        die(`wasm-ld ${p}: ${w.err}`);
    const theirs = readFileSync(join(L.tmp, "out.wasm"));
    const r = L.ld(["--no-import-memory", ...base, "-o", "out.wasm"]);
    if (r.status !== 0)
        die(`ld ${p}: ${r.err}`);
    const ours = Buffer.from(H.store.files.get("/tmp/out.wasm"));
    const at = ours.length - 28;
    if (at < 0 || !ours.subarray(at, at + 8).equals(tail) || !ours.subarray(0, at).equals(theirs))
        bad.push(`ld's ${p} is not wasm-ld's`);
}

// The transcript.
let transcript = "";
for (const c of CASES) {
    const r = sh(`./${c}`);
    transcript += `$ ${c}\n${r.out}`;
    if (r.err)
        transcript += `stderr: ${r.err}`;
    transcript += `status ${r.status}\n`;
}
if (process.env.BLESS)
    writeFileSync(GOLDEN, transcript);
else if (transcript !== readFileSync(GOLDEN, "utf8"))
    bad.push(`the transcript is not ${GOLDEN}; run with BLESS=1 and diff`);

// cat over reads of 512 bytes, and bytes that are not text.
const bytes = new Uint8Array(3000);
for (let i = 0, x = 1; i < bytes.length; i++, x = (x * 1103515245 + 12345) >>> 0)
    bytes[i] = x >>> 24;
plant(H, "/tmp/ex/in", bytes);
const c = sh("./cat <in >copy");
const copy = H.store.files.get("/tmp/ex/copy");
if (c.status !== 0 || !copy || Buffer.compare(Buffer.from(copy), Buffer.from(bytes)) !== 0)
    bad.push(`cat: status ${c.status}, and ${copy?.length ?? 0} bytes not the 3000 given`);

// 5 MB of primes through crt.s's buffer, doubled as it fills.
const p = sh("./primes 10000000 | wc");
if (p.out.trim().split(/\s+/).join(" ") !== "66458 664579 5227116")
    bad.push(`primes 10000000 | wc: ${p.out}`);

if (bad.length)
    die(bad.join("\n  "));
console.log(`examples ok: ${PROGRAMS.length} programs built by build.sh, linked as wasm-ld does, ` +
            `and ${CASES.length + 2} runs`);
