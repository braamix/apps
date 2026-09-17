// What test/stdlib cannot say, because it needs a stream, a signal or the end
// of the process: sys.stdin and input() over a redirected file, files still
// open at exit, and signals delivered to a handler while the program sleeps.
// Each expected text is what CPython 3.16 printed for the same program.

import { readFileSync, readdirSync, statSync } from "node:fs";
import { join, relative } from "node:path";

import { boot, put, get, run, same, ok, die, opt, CORE, LIB, STORE_LIB, H } from "./pylib.mjs";

await boot("pyio");
let bad = 0;
const check = (what, got, want) => {
    if (!same(what, got, want)) bad++;
};

// ------------------------------------------------------------------ stdin

{
    const src = `import sys
print(sys.stdin.isatty(), sys.stdin.encoding, sys.stdin.fileno(), sys.stdin.mode, sys.stdin.name)
print(sys.stdout.name, sys.stderr.name, sys.stdout.errors, sys.stderr.errors, sys.stdin.errors)
print(repr(input()), repr(input("prompt> ")))
print(repr(sys.stdin.readline()), repr(sys.stdin.read(4)))
print(list(sys.stdin))
try:
    input()
except EOFError as e:
    print("EOF", e)
print(repr(sys.stdin.read()), sys.stdin.buffer.read())
sys.stdout.write("no newline")
sys.stderr.write("to stderr\\n")
print(" and more", end="", file=sys.stdout)
print(sys.__stdout__ is sys.stdout, sys.stdout.line_buffering, sys.stdout.buffer.raw.name)
sys.stdout.flush()
sys.stdout.buffer.write(b"bytes\\n")
`;
    put("/tmp/c.py", src);
    const r = run("/tmp/c.py", "first line\nsecond é\nthird\nfourth\nfifth\n");
    check("stdin", r.out,
          "False utf-8 0 r <stdin>\n" +
          "<stdout> <stderr> surrogateescape backslashreplace surrogateescape\n" +
          "prompt> 'first line' 'second é'\n" +
          "'third\\n' 'four'\n" +
          "['th\\n', 'fifth\\n']\n" +
          "EOF EOF when reading a line\n" +
          "'' b''\n" +
          "no newline and moreTrue False <stdout>\n" +
          "bytes\n");
    check("stdin's stderr", r.err, "to stderr\n");
}

// A program read from stdin itself, and the bytes a UTF-8 decoder refuses.
{
    const r = run("-", "import sys\nprint(repr(sys.stdin.read()))\n");
    check("a program on stdin", r.out, "''\n");
    put("/tmp/c.py", "import sys\nprint(ascii(sys.stdin.read()))\n");
    const b = run("/tmp/c.py", new Uint8Array([0x61, 0xff, 0x0a]));
    check("undecodable stdin", b.out, "'a\\udcff\\n'\n");
}

// ------------------------------------------------------------ at exit

// What is still open is flushed and closed, and a TemporaryFile's file goes.
{
    put("/tmp/c.py", `import tempfile
f = open("/tmp/left.txt", "w")
f.write("kept\\n")
g = open("/tmp/left.bin", "wb", buffering=100)
g.write(b"bin")
t = tempfile.TemporaryFile(dir="/tmp")
t.write(b"x")
w = open("/tmp/wrapped.txt", "w", encoding="utf-16")
w.write("é")
print("done", end="")
`);
    const r = run("/tmp/c.py");
    check("exit output", r.out + r.err, "done");
    check("an unclosed text file", get("/tmp/left.txt"), "kept\n");
    check("an unclosed binary file", get("/tmp/left.bin"), "bin");
    const w = H.store.files.get("/tmp/wrapped.txt");
    check("an unclosed utf-16 file", Array.from(w ?? []).join(","), "255,254,233,0");
    const left = [...H.store.files.keys()].filter((p) => p.includes(".pytmp-"));
    check("TemporaryFile at exit", left.join(" "), "");
}

// os._exit leaves at once: nothing is flushed.
{
    put("/tmp/c.py", `import os
f = open("/tmp/lost.txt", "w")
f.write("never")
os._exit(3)
`);
    const r = run("/tmp/c.py");
    check("os._exit status", String(r.status), "3");
    check("os._exit drops the buffer", get("/tmp/lost.txt"), "");
}

// An exception at exit still closes the files.
{
    put("/tmp/c.py", `f = open("/tmp/raised.txt", "w")
f.write("flushed anyway")
raise SystemExit(4)
`);
    const r = run("/tmp/c.py");
    check("SystemExit status", String(r.status), "4");
    check("SystemExit flushes", get("/tmp/raised.txt"), "flushed anyway");
}

// ------------------------------------------------------------- signals

// A kernel of its own for each, with the library planted: the program runs in
// the background, and `sleep 3; kill` is read while it sleeps. Braam acts on a
// process only where it parks, which a sleeping one does. The clock is driven
// here in small steps: a jump straight to the next timer would leave the shell
// holding the second line until the program's sleep was over.
async function signalled(source, sig) {
    const K = await import(join(CORE, "test/system/harness.mjs"));
    await K.init(opt.kernel, opt.rootfs);
    K.kernel().init(0);
    if (K.run(0) !== -1) die("the shell did not park on the keyboard");
    K.regrid(80, 24, "resize returned no screen descriptor");
    K.store.files.set("/bin/py", new Uint8Array(readFileSync(opt.binary)));
    const plant = (at) => {
        for (const e of readdirSync(at)) {
            const p = join(at, e);
            if (statSync(p).isDirectory()) { plant(p); continue; }
            if (!p.endsWith(".py")) continue;
            const dst = `${STORE_LIB}/${relative(LIB, p)}`;
            for (let i = dst.indexOf("/", 1); i > 0; i = dst.indexOf("/", i + 1))
                K.store.dirs.add(dst.slice(0, i));
            K.store.files.set(dst, new Uint8Array(readFileSync(p)));
        }
    };
    plant(LIB);
    K.store.files.set("/tmp/c.py", new TextEncoder().encode(source));
    for (const line of ["py /tmp/c.py >/tmp/o 2>&1 &", `sleep 3; kill -${sig} %1`]) {
        K.type(line);
        K.press(K.KEY.ENTER);
    }
    let now = 100;
    for (let n = 0; ; n++) {
        const d = K.kernel().tick(now);
        const busy = K.net.drain();
        if (d === -1 && !busy) break;
        if (n > 200000) die("the signalled program never finished");
        now += d > 0 ? Math.min(d, 10) : 1;
    }
    const out = new TextDecoder().decode(K.store.files.get("/tmp/o") ?? new Uint8Array());
    // How the job ended, as the shell reports it.
    const job = K.rows(K.screen()).map((l) => /^\[1\] (\S+)/.exec(l)).filter(Boolean).pop();
    return { out, job: job ? job[1] : "?" };
}

// Every program imports signal first: the imports park, and the shell reads
// its second line in between.
const HANDLED = `import signal, time
def handler(signum, frame):
    print("got", signal.Signals(signum).name, frame.f_code.co_name, flush=True)
signal.signal(signal.SIGTERM, handler)
signal.signal(signal.SIGINT, handler)
print("start", flush=True)
def main():
    time.sleep(10)
    print("slept on")
main()
`;

for (const sig of ["TERM", "INT"]) {
    const r = await signalled(HANDLED, sig);
    check(`SIG${sig} to a handler`, r.out, `start\ngot SIG${sig} main\nslept on\n`);
}
{
    const r = await signalled(`import signal, time
signal.signal(signal.SIGTERM, signal.SIG_IGN)
print("start", flush=True)
time.sleep(10)
print("ignored")
`, "TERM");
    check("SIGTERM ignored", r.out, "start\nignored\n");
}
{
    const r = await signalled(`import signal, time
def handler(signum, frame):
    raise TimeoutError("out of the handler")
signal.signal(signal.SIGTERM, handler)
print("start", flush=True)
try:
    time.sleep(10)
except TimeoutError as e:
    print("caught:", e)
`, "TERM");
    check("an exception out of a handler", r.out, "start\ncaught: out of the handler\n");
}
{
    // No handler: the kernel's default action, which is to end the process.
    const r = await signalled(`import signal, time
print("start", flush=True)
time.sleep(10)
print("not reached")
`, "TERM");
    check("SIGTERM's default action", r.out, "start\n");
    check("SIGTERM's default action, the job", r.job, "interrupt");
}
{
    // ^C's default is KeyboardInterrupt, raised out of the sleep.
    const r = await signalled(`import signal, time
print("start", flush=True)
time.sleep(10)
`, "INT");
    // The frame and the exception; the source line is linecache's.
    const lines = r.out.split("\n").filter((l) => !l.startsWith("    "));
    check("SIGINT's default action", lines.join("\n"),
          "start\n" +
          "Traceback (most recent call last):\n" +
          '  File "/tmp/c.py", line 3, in <module>\n' +
          "KeyboardInterrupt\n");
    check("SIGINT's default action, the job", r.job, "interrupt");
}

if (bad) {
    console.error(`\npyio: ${bad} checks failed`);
    process.exit(1);
}
ok("stdin, the exit flush and delivered signals behave as CPython's");
