// The read-eval-print loop: what a session looks like at the console, and what
// it writes down a pipe. The prompts and the banner are stderr's, as they are
// in CPython, so a redirected stdout holds what the commands printed and
// nothing else -- which is what makes the transcripts below comparable.

import { boot, run, put, get, same, golden, die, H } from "./pylib.mjs";

await boot("pyrepl");
let bad = 0;
const check = (what, got, want) => {
    if (!same(what, got, want)) bad++;
};

// ------------------------------------------------------------- down a pipe

// `python -i` reads the prompt from whatever stdin is, which is how a session
// can be written down and compared. Without -i a pipe is a program, not a
// session, and that is CPython's rule too.
const session = (lines) => run("-i", lines.join("\n") + "\n");

{
    const r = session(["1+1", "x = 'hi'", "print(x * 2)", "_ * 2"]);
    // The value of an expression is printed and kept in _; an assignment is
    // not an expression and prints nothing.
    check("displayhook and _", r.out, "2\nhihi\n4\n");
    check("the prompts are stderr's", r.err, ">>> >>> >>> >>> >>> \n");
    check("status", String(r.status), "0");
}

{
    // A suite stays open until a line that ends it, which at a prompt is an
    // empty one. ps2 is the prompt while it is open.
    const r = session(["if 1:", "  print('in')", "", "for i in range(2):", "  i", ""]);
    check("a suite over several lines", r.out, "in\n0\n1\n");
    check("ps2 while the suite is open", r.err, ">>> ... ... >>> ... ... >>> \n");
}

{
    const r = session(["None", "print(None)", "sys"]);
    check("None prints nothing; a NameError is not the end", r.out, "None\n");
    check(
        "the traceback",
        r.err,
        ">>> >>> >>> Traceback (most recent call last):\n" +
            '  File "<stdin>", line 1, in <module>\n' +
            "NameError: name 'sys' is not defined\n" +
            ">>> \n"
    );
}

{
    const r = session(["x ===", "print('after')"]);
    check("a SyntaxError names the line and points at the column", r.err,
        '>>>   File "<stdin>", line 1\n' +
        "    x ===\n" +
        "        ^\n" +
        "SyntaxError: invalid syntax\n" +
        ">>> >>> \n");
    check("and the session goes on", r.out, "after\n");
}

{
    // sys.ps1 is a name like any other: setting it changes the prompt.
    const r = session(["import sys", "sys.ps1 = '$ '", "1"]);
    check("sys.ps1", r.err, ">>> >>> $ $ \n");
}

{
    const r = session(["import sys", "sys.exit(3)", "print('no')"]);
    check("sys.exit leaves the session", r.out, "");
    check("with its status", String(r.status), "3");
    check("and no prompt after it", r.err, ">>> >>> ");
}

{
    // atexit belongs to the session, not to each command: a command ending is
    // not the program ending.
    const r = session(["import atexit", "atexit.register(print, 'bye')", "1"]);
    check("atexit runs once, at the end", r.out, "<built-in function print>\n1\nbye\n");
}

// ------------------------------------------------------------------- `-m`

{
    put("/home/mod.py", "import sys\nprint(__name__, sys.argv)\n");
    const r = run("-m mod a b");
    check("-m runs the module as __main__", r.out, "__main__ ['/home/mod.py', 'a', 'b']\n");
    check("and everything after it is the module's", r.err, "");
}

{
    const r = run("-m nosuch");
    // runpy names sys.executable, which is where PATH found this binary.
    check("a module that is not there", r.err, "/bin/py: No module named nosuch\n");
    check("status", String(r.status), "1");
}

// --------------------------------------------------------- at the console

// With a terminal the line is editable: the editor holds the keyboard while a
// command is typed and gives it back the moment one runs, so a ^C during a
// command reaches the process as a signal rather than as a keystroke.
let clock = 1;
function settle() {
    let now = (clock += 100);
    for (let d = H.run(now), i = 0; d !== -1; d = H.run(now))
        if ((now += d > 0 ? d : 1), ++i > 200000) die("the session did not settle");
    clock = now;
}
function typed(text) {
    H.type(text);
    H.press(H.KEY.ENTER);
    settle();
}
function screen() {
    const s = H.screen();
    const rows = [];
    for (let y = 0; y <= s.cursor_y; y++) rows.push(H.row(s, y).replace(/\s+$/, ""));
    return rows.join("\n");
}

typed("py");
typed("1+1");
typed("'é' + 'x'"); // a codepoint past ASCII is one cell, and echoes as one
typed("if 1:");
typed("  print('in')");
typed("");
H.press(H.KEY.UP); // the line before this one, back from the history
settle();
H.press(H.KEY.ENTER);
settle();
H.type("1 + ");
for (let i = 0; i < 4; i++) H.press(H.KEY.BACKSPACE); // "1 + " typed away
settle();
typed("'edited'");
H.press("d".codePointAt(0), H.CTRL); // ^D on an empty line ends the session
settle();

const transcript = screen();
if (!transcript.includes("home $ py")) die(`python did not start:\n${transcript}`);
golden("repl.log", transcript.slice(transcript.indexOf("home $ py")));

process.exit(bad ? 1 : 0);
