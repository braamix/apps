// The VM and the driver between them: the three ways a program arrives, what
// it prints, what an error leaves on stderr, and the status the shell sees.
//
// The upstream suite (runcases.mjs) says what the language does; this says
// what the *program* does around it.

import { boot, put, run, script, ok, die, same } from "./pylib.mjs";

await boot("pyvm");

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// A program on stdin, which is the third way in.
{
    put("/tmp/s.py", "print('from stdin')\n");
    const r = run("- </tmp/s.py");
    check("stdin stdout", r.out, "from stdin\n");
    check("stdin status", String(r.status), "0");
}

// sys.argv is the script and then the rest of the command line.
{
    const r = script("import sys\nprint(sys.argv)\n", null);
    check("argv", r.out, "['/tmp/c.py']\n");
}
{
    put("/tmp/c.py", "import sys\nprint(sys.argv)\n");
    const r = run("/tmp/c.py a b");
    check("argv with arguments", r.out, "['/tmp/c.py', 'a', 'b']\n");
}

// An uncaught error: the traceback names the file, the line and the scope,
// stdout still arrives, and the status is 1.
{
    const r = script("print('before')\ndef f():\n    return 1 // 0\nf()\n");
    check("error stdout", r.out, "before\n");
    check("error stderr", r.err,
          "Traceback (most recent call last):\n" +
          '  File "/tmp/c.py", line 4, in <module>\n' +
          '  File "/tmp/c.py", line 3, in f\n' +
          "ZeroDivisionError: integer division or modulo by zero\n");
    check("error status", String(r.status), "1");
}

// A name that is not there, and a local read before it is written.
{
    const r = script("print(nope)\n");
    if (!r.err.endsWith("NameError: name 'nope' is not defined\n"))
        die(`a missing global: ${JSON.stringify(r.err)}`);
}
{
    const r = script("def f():\n    x = x + 1\nf()\n");
    if (!r.err.endsWith("UnboundLocalError: local variable 'x' referenced before assignment\n"))
        die(`a local read too early: ${JSON.stringify(r.err)}`);
}

// A syntax error never reaches the VM, and says where it is -- in the shape a
// traceback has, which is what CPython prints for one too.
{
    const r = script("x = (1\n");
    check("syntax error stdout", r.out, "");
    if (!r.err.startsWith('  File "/tmp/c.py", line ') || !r.err.includes("\nSyntaxError: "))
        die(`a syntax error without a place: ${JSON.stringify(r.err)}`);
    check("syntax error status", String(r.status), "1");
}

// Deep recursion is a RecursionError rather than a trap: the frame stack is
// ours, so the limit is ours to report.
{
    const r = script("def f(n):\n    return f(n + 1)\nf(0)\n");
    if (!r.err.includes("RecursionError"))
        die(`deep recursion did not stop cleanly: ${JSON.stringify(r.err.slice(-120))}`);
    check("recursion status", String(r.status), "1");
}

// More output than one write holds, so the driver is asked more than once.
{
    const r = script("for i in range(2000):\n    print(i)\n");
    const want = Array.from({ length: 2000 }, (_, i) => i).join("\n") + "\n";
    check("a long run of output", r.out, want);
}

// A closure over a loop variable, which is the collector's problem as much as
// the compiler's: the cells outlive the frames that made them.
{
    const r = script(
        "def make():\n" +
        "    out = []\n" +
        "    for i in range(5):\n" +
        "        def get(n=i):\n" +
        "            return n * n\n" +
        "        out = out + [get]\n" +
        "    return out\n" +
        "fs = make()\n" +
        "print([f() for f in fs])\n");
    check("closures", r.out, "[0, 1, 4, 9, 16]\n");
}

// Enough garbage to collect several times while a program runs.
{
    const r = script(
        "n = 0\n" +
        "for i in range(3000):\n" +
        "    d = {'a': [i, i + 1], 'b': (i,)}\n" +
        "    n = n + d['a'][1]\n" +
        "print(n)\n");
    check("collecting under load", r.out, "4501500\n");
}

// An uncaught exception that was raised, rather than one the runtime found:
// the traceback carries the exception's own arguments.
{
    const r = script("def f():\n    raise ValueError('nope', 2)\nf()\n");
    check("raise stderr", r.err,
          "Traceback (most recent call last):\n" +
          '  File "/tmp/c.py", line 3, in <module>\n' +
          '  File "/tmp/c.py", line 2, in f\n' +
          "ValueError: ('nope', 2)\n");
    check("raise status", String(r.status), "1");
}

// A chained one prints both halves, the way CPython does.
{
    const r = script("try:\n" +
                     "    1 / 0\n" +
                     "except ZeroDivisionError:\n" +
                     "    raise ValueError('second')\n");
    if (!r.err.includes("During handling of the above exception"))
        die(`the context was not printed: ${JSON.stringify(r.err)}`);
    if (!r.err.startsWith("Traceback"))
        die(`the first traceback is missing: ${JSON.stringify(r.err)}`);
}

// SystemExit is the one exception that is not an error: it sets the status
// and prints nothing.
{
    const r = script("import sys\nprint('bye')\nsys.exit(3)\n");
    check("sys.exit stdout", r.out, "bye\n");
    check("sys.exit stderr", r.err, "");
    check("sys.exit status", String(r.status), "3");
}
{
    const r = script("raise SystemExit\n");
    check("bare SystemExit status", String(r.status), "0");
    check("bare SystemExit stderr", r.err, "");
}
{
    const r = script("import sys\nsys.exit('gone wrong')\n");
    check("SystemExit message", r.err, "gone wrong\n");
    check("SystemExit message status", String(r.status), "1");
}

// And the same again collecting at *every* allocation, which is what turns a
// missing Root in the VM into a wrong answer rather than a rare crash.
{
    put("/tmp/c.py",
        "def pairs(n):\n" +
        "    return [(i, str(i), [i] * 2) for i in range(n)]\n" +
        "d = {}\n" +
        "for a, b, c in pairs(12):\n" +
        "    d[b] = a + c[0] + c[1]\n" +
        "print(sum([d[k] for k in d]), len(d), d['11'])\n");
    const r = run("/tmp/c.py", null, "PY_GC_STRESS=1");
    check("under gc stress", r.out + r.err, "198 12 33\n");
}

if (bad) {
    console.error(`pyvm: ${bad} checks failed`);
    process.exit(1);
}
ok("the driver, argv, tracebacks and the collector under load");
