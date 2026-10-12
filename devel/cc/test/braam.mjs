// The programs of vcc's braam_tests.cpp, compiled by cc on Braam and run
// there: streams, files, stdin, tasks, signals and poll. Upstream runs them on
// a fake kernel under node, and each case says where Braam's answer is not
// that kernel's. extract.py names a program after the TEST it is found in, so
// a source shared by tests is named after the first that follows it: Status.c
// is cat, Cat.c is wc, and PromptFlush_2.c is TaskBytes' recursion.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { TREE, fixture } from "./ccfix.mjs";

const { die, boot } = fixture("braam");
const { H, put, text, sh, submit } = await boot();
const dir = join(TREE, "test/programs/braam_tests");

let cat = "";
for (let i = 1; i <= 3000; i++)
    cat += i + (i % 7 ? " " : "\n");
let rest = "";
for (let i = 0; i < 400; i++)
    rest += (i % 10) + (i % 50 === 49 ? "\n" : "");
let stdin_stream = "? [a long f] [irst lin] [e\n] \n";
for (const c of rest)
    stdin_stream += c === "\n" ? "|" : c;
stdin_stream += `\n${rest.length} eof\n`;
let data = "";
for (let i = 0; i < 5000; i++)
    data += String.fromCharCode(97 + (i % 23));
put("/tmp/data", data);

// wc: upstream's source uses strerror and errno without their headers.
const WC = "#include <errno.h>\n#include <string.h>\n";

// [program, arguments, stdin, stdout, status, stderr, lines put before the source]
const CASES = [
    ["Hello", "one 'two words'", "", "hello from ./Hello: 3 args\n  one\n  two words\n", 0],
    ["Status", "", cat, cat, 0],
    ["Status", "", "", "", 0],
    ["Cat", "", "one two three\nfour five six\n", "2 6 28\n", 0, "", WC],
    ["Cat", "data", "", "0 1 5000\n", 0, "", WC],
    ["Cat", "none", "", "", 1, "wc: none: Not found\n", WC],
    // The harness's clock stands still, so no time passes in a sleep.
    ["Sleep", "", "", "before\nafter 0\n", 0],
    // exit() writes out what the streams hold (Braam's C library, not upstream's).
    ["Exit", "", "", "lost\n", 7],
    ["AllocaInFunction", "", "", "sum 4950\n", 0],
    ["Malloc", "", "", "ok 1\n", 0],
    ["PromptFlush", "", "ada\n", "name? hello, ada\n", 0],
    // The trap's message is on stdout here, and the status the kernel's.
    ["PromptFlush_2", "", "", "coroutine trap: CO_TRAP_NO_SPACE: depth\n", 255],
    ["Files", "d", "",
     "size 19 reg 1 dir 0\n1: one\n2: two\n3: three\n4: four\neof 1 err 0\nfirst o again o\n" +
     "tell 13\nread 5 '\nfour'\nfstat size 19\nappended 24\nold -1 new 0\n" +
     "rmdir full -1 Not empty\ncwd ends /d\nunlink 0 rmdir 0 rmdir 0\n", 0],
    ["FileErrors", "", "", "fopen 1 Not found\nmode 1 22\nstat -1 35\nfstat -1\n", 0,
     "braam-FileErrors.none: Not found\n"],
    ["StdinStream", "", "a long first line\n" + rest, stdin_stream, 0],
    ["CopyFile", "data", "", "5000 5000 same\n", 0],
    // nap(9) sleeps (unsigned)-10 ms, which the fake kernel takes for no time,
    // and Braam for longer than main's 200 ms.
    ["Tasks", "", "",
     "spawned 1 2 3 4 5 6 7, then 0\ndone 7\ndone 6\ndone 5\ndone 4\ndone 3\ndone 2\n" +
     "done 1\nafter destroy: 1\n", 0],
    ["Yield", "", "", "yields 0, counter 5\n", 0],
    // POLLIN on standard output is Err(Invalid) on Braam, so every poll fails.
    ["Poll", "", "x", "-1: 0 0\n-1: 0\n-1 1\n", 0],
];

const bad = [];
const built = new Set();
function build(n, prefix = "") {
    if (built.has(n))
        return true;
    put(`/tmp/${n}.c`, prefix + readFileSync(join(dir, n + ".c"), "utf8"));
    const r = sh(`cc ${n}.c -o ${n}`);
    if (r.status !== 0) {
        bad.push(`${n}: cc: ${r.err}`);
        return false;
    }
    built.add(n);
    return true;
}
const show = (r) => JSON.stringify(r);

for (const [n, args, stdin, out, status, err = "", prefix = ""] of CASES) {
    if (!build(n, prefix))
        continue;
    put("/tmp/in", stdin);
    const r = sh(`./${n} ${args} <in`);
    if (r.out !== out || r.status !== status || r.err !== err)
        bad.push(`${n} ${args}: ${show(r)}, expected ${show({ out, err, status })}`);
}

// docs/examples/notes.c, through the session Braam_Example.md shows.
put("/tmp/notes.c", new Uint8Array(readFileSync(join(TREE, "docs/examples/notes.c"))));
let r = sh("cc notes.c -o notes");
if (r.status !== 0)
    bad.push(`notes.c: ${r.err}`);
for (const [args, stdin, out, status, err = ""] of [
         ["", "", "no notes\n", 0],
         ["add buy milk", "", "", 0],
         ["add call the plumber", "", "", 0],
         ["ask", "water plants\n\nwrite letter\n", "note? note? note? note? \n2 added\n", 0],
         ["del 2", "", "", 0],
         ["del 9", "", "there is no note 9\n", 1],
         ["", "", "  1  buy milk\n  2  water plants\n  3  write letter\n(3 notes, 35 bytes)\n", 0],
     ]) {
    put("/tmp/in", stdin);
    r = sh(`./notes ${args} <in`);
    if (r.out !== out || r.status !== status || r.err !== err)
        bad.push(`notes ${args}: ${show(r)}, expected ${show({ out, err, status })}`);
}
r = sh("./notes bogus");
if (r.out !== "" || r.status !== 2 || !r.err.includes("usage: notes"))
    bad.push(`notes bogus: ${show(r)}`);

// Refused at compile time.
for (const [n, want] of [["AllocaInMain", "'alloca' in a coroutine"]]) {
    put(`/tmp/${n}.c`, new Uint8Array(readFileSync(join(dir, n + ".c"))));
    const r = sh(`cc -c ${n}.c`);
    if (r.status !== 1 || !r.err.includes(want))
        bad.push(`${n}: ${show(r)}, expected ${JSON.stringify(want)}`);
}
put("/tmp/plain.c", "int main(void) { return 0; }\n");
r = sh("cc -c plain.c");
if (r.status !== 1 || !r.err.includes("on Braam, main is coro(braam_call *)"))
    bad.push(`a plain main: ${show(r)}`);

// The signals, from the keyboard: ^C while the program reads the console.
const ctrl_c = () => {
    H.press("c".codePointAt(0), H.CTRL);
    submit("");
};
if (build("SignalDefault")) {
    submit("./SignalDefault >o 2>e");
    ctrl_c();
    submit("echo $? >s");
    if (text("/tmp/o") !== "waiting\n" || text("/tmp/s") !== "130\n")
        bad.push(`SignalDefault: ${show({ out: text("/tmp/o"), status: text("/tmp/s") })}`);
}
// The read and the ticker's sleep give up, and the ticker's line follows
// main's here; a line typed is one read, and ^D ends the input.
if (build("SignalCaught")) {
    submit("./SignalCaught >o 2>e", false);
    H.press("c".codePointAt(0), H.CTRL);
    submit("abc", false);
    H.press("d".codePointAt(0), H.CTRL);
    submit("echo $? >s");
    const want = "spawned 1\ninterrupted, took 1, pending 0\nticker: Interrupted\nread 4\n" +
                 "ticker done 1 result 42\n";
    if (text("/tmp/o") !== want || text("/tmp/s") !== "0\n")
        bad.push(`SignalCaught: ${show({ out: text("/tmp/o"), status: text("/tmp/s") })}`);
}

if (bad.length)
    die(bad.join("\n"));
console.log(`braam ok: ${CASES.length} runs of ${built.size} programs, notes.c's session, ` +
            "2 refusals and 2 signals");
