// That the program starts, answers its command line, and reports the status
// the shell sees.

import { boot, put, rm, run, ok, die, same } from "./pylib.mjs";

const VERSION = "Python 3.14.0 on Braam\n";

await boot("pysmoke");

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// The banner. A bare `py` is the prompt now, not the banner: pyrepl.mjs has
// that.
for (const tail of ["-V", "--version"]) {
    const r = run(tail);
    check(`\`py ${tail}\` stdout`, r.out, VERSION);
    check(`\`py ${tail}\` stderr`, r.err, "");
    check(`\`py ${tail}\` status`, String(r.status), "0");
}

// Asked for: stdout and 0.
{
    const r = run("-h");
    if (!r.out.startsWith("Usage:\n    python  "))
        die(`-h did not print the usage block: ${JSON.stringify(r.out.slice(0, 60))}`);
    if (!r.out.includes("TODO.md"))
        die("the usage block does not say what is not there yet");
    check("`py -h` stderr", r.err, "");
    check("`py -h` status", String(r.status), "0");
}

// Got wrong: stderr and 2.
{
    const r = run("-q");
    check("`py -q` stdout", r.out, "");
    if (!r.err.startsWith("python: unknown option -q\n"))
        die(`-q did not name the option: ${JSON.stringify(r.err.slice(0, 60))}`);
    if (!r.err.includes("Usage:"))
        die("the usage block did not follow the complaint");
    check("`py -q` status", String(r.status), "2");
}

// A valued letter with nothing after it.
{
    const r = run("-c");
    check("`py -c` stdout", r.out, "");
    if (!r.err.startsWith("python: option -c takes a value\n"))
        die(`bare -c did not say so: ${JSON.stringify(r.err.slice(0, 60))}`);
    check("`py -c` status", String(r.status), "2");
}

// The two ways in, each running the same program.
{
    const r = run("-c 'print(1)'");
    check("`py -c print(1)` stdout", r.out, "1\n");
    check("`py -c print(1)` stderr", r.err, "");
    check("`py -c print(1)` status", String(r.status), "0");
}
{
    put("/tmp/c.py", "print(2)\n");
    const r = run("/tmp/c.py");
    check("`py /tmp/c.py` stdout", r.out, "2\n");
    check("`py /tmp/c.py` stderr", r.err, "");
    check("`py /tmp/c.py` status", String(r.status), "0");
}

// A file that is not there is the shell's `Input` complaining, not ours.
{
    rm("/tmp/c.py");
    const r = run("/tmp/c.py");
    check("`py <missing>` stdout", r.out, "");
    check("`py <missing>` stderr", r.err, "python: /tmp/c.py: not found\n");
    check("`py <missing>` status", String(r.status), "1");
}

if (bad) {
    console.error(`pysmoke: ${bad} checks failed`);
    process.exit(1);
}
ok("the command line, the banner and the statuses");
