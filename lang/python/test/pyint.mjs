// ^C reaches a running program, and a program may catch it.
//
// `kill -INT` rather than a typed ^C, for the reason the benchmarks' own
// interrupt tests give: run() pumps the kernel until it is idle, and a
// foreground program that computes without parking never is, so there is no
// window to type in. A background job with a second command line queued behind
// it gives the shell its turn where the VM parks -- which, for a compute loop,
// is only at the end of a burst.

import { readFileSync } from "node:fs";
import { join } from "node:path";

import { boot, opt, ok, die, CORE } from "./pylib.mjs";

// boot() only to be told what is missing to build; each case then takes a
// kernel of its own, because the interrupt needs both command lines queued
// before the first tick.
await boot("pyint");

async function interrupted(source) {
    const K = await import(join(CORE, "test/system/harness.mjs"));
    await K.init(opt.kernel, opt.rootfs);
    K.kernel().init(0);
    if (K.run(0) !== -1) die("the shell did not park on the keyboard");
    K.regrid(80, 24, "resize returned no screen descriptor");
    K.store.files.set("/bin/py", new Uint8Array(readFileSync(opt.binary)));
    K.store.files.set("/tmp/c.py", new TextEncoder().encode(source));

    for (const line of ["py /tmp/c.py &", "kill -INT %1"]) {
        K.type(line);
        K.press(K.KEY.ENTER);
    }
    if (K.run(100) !== -1) die("the program left the kernel with work to do");
    return K.rows(K.screen()).join("\n").replace(/[ \t]+$/gm, "");
}

// Nothing here parks, so only the burst bound lets the signal through at all.
// Nothing is printed before the interrupt either: the shell echoes the second
// command line over the same screen row, and what that looks like is the
// terminal's business rather than this test's.
{
    const out = await interrupted("n = 0\nwhile True:\n    n = n + 1\n");
    if (!out.includes("KeyboardInterrupt"))
        die(`^C did not reach the loop: ${JSON.stringify(out)}`);
    if (!out.includes("line 3"))
        die(`the traceback does not name the loop: ${JSON.stringify(out)}`);
}

// And a program may catch it, because it is an exception like any other.
{
    const out = await interrupted(
        "try:\n" +
        "    n = 0\n" +
        "    while True:\n" +
        "        n = n + 1\n" +
        "except KeyboardInterrupt:\n" +
        "    print('caught it')\n");
    if (!out.includes("caught it"))
        die(`the program did not catch its own ^C: ${JSON.stringify(out)}`);
    if (out.includes("Traceback"))
        die(`a caught interrupt still printed a traceback: ${JSON.stringify(out)}`);
}

// An endless generator drained by a builtin is the case where the burst bound
// could have been lost: sum() parks once and everything after it happens
// inside the continuation. Each item still costs a turn of the loop, so the
// burst ends and the signal arrives.
{
    const out = await interrupted(
        "def forever():\n" +
        "    n = 0\n" +
        "    while True:\n" +
        "        yield n\n" +
        "        n = n + 1\n" +
        "try:\n" +
        "    sum(forever())\n" +
        "except KeyboardInterrupt:\n" +
        "    print('caught in a generator')\n");
    if (!out.includes("caught in a generator"))
        die(`^C did not reach a drained generator: ${JSON.stringify(out)}`);
}

ok("^C becomes KeyboardInterrupt, and a program may catch it");
