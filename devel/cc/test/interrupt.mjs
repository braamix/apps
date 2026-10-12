// ^C in a long lower run: lower parks once per declaration, so a signal
// reaches it mid-run, and its default action ends it, with nothing written;
// the shell reports that status, 130, as `interrupt`.
//
// `kill -INT` rather than a typed ^C, as the benchmarks' interrupt tests do:
// the kernel runs until it idles, so the second line has to be queued behind
// the first, and the shell takes its turn where lower parks.

import { fixture } from "./ccfix.mjs";

const { die, boot } = fixture("interrupt");
const { H, put, text, sh, submit, store } = await boot();

// A source of many declarations.
const N = 2000;
let src = "";
for (let i = 0; i < N; i++)
    src += `int f${i}(int x) { return x * ${i} + ${i % 7}; }\n`;
put("/tmp/big.c", src);
let r = sh(`L=${store}/libexec\n$L/vcpp big.c big.i && $L/vparse big.i big.ast`);
if (r.status !== 0)
    die(`vcpp and vparse: ${r.err}`);
submit(`ln -s ${store}/libexec/vlower vlower`);

// Uninterrupted, it finishes.
r = sh("./vlower big.ast big.tac");
if (r.status !== 0 || !H.store.files.has("/tmp/big.tac"))
    die(`vlower: ${JSON.stringify(r)}`);
H.store.files.delete("/tmp/big.tac");

// Both lines before the first tick, so the second is read while the first runs.
for (const line of ["./vlower big.ast big.tac &", "kill -INT %1"]) {
    H.type(line);
    H.press(H.KEY.ENTER);
}
submit("");
const shown = H.rows(H.screen()).filter((l) => l.trim());
if (!shown.includes("[1] interrupt ./vlower big.ast big.tac"))
    die(`vlower was not interrupted: ${JSON.stringify(shown.slice(-6))}`);
// The output is opened at the start and written whole at the end.
if (H.store.files.get("/tmp/big.tac")?.length)
    die("an interrupted vlower wrote its output");
console.log(`interrupt ok: vlower over ${N} functions, interrupted by kill -INT`);
