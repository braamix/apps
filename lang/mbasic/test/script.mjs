// `mbasic prog.bas` -- run a file and exit.
//
// The file's lines are read as if typed, so a numbered line is stored and an
// unnumbered one runs at once; but there is no banner and no Ok, the program
// is RUN when the lines run out unless it ran itself, and the process exits
// when that run ends. INPUT reads stdin, which is a different stream from the
// file -- that is what makes `mbasic guess.bas` playable.
//
// Redirected stdin is the other thing entirely and is unchanged: repl.mjs and
// the rest drive `mb </tmp/i`, banner and all.

import { boot, script, golden, die, ok } from "./mblib.mjs";

const H = await boot("script");

const is = (what, got, want) => {
    if (got !== want)
        die(`${what}:\n  want ${JSON.stringify(want)}\n  got  ${JSON.stringify(got)}`);
};

// The reported bug: a program, and only its own output.
let r = script('10 print "Hello, world!"\n20 end\n');
is("a plain program", r.out, "Hello, world!\r\n");
is("its status", r.status, 0);

// A file that runs itself is not run twice.
r = script('10 print "once"\nrun\n');
is("an explicit run", r.out, "once\r\n");

// ...and one that runs itself and then goes on being a session.
r = script('10 print "a"\nrun\n20 print "b"\nrun\n');
is("two explicit runs", r.out, "a\r\na\r\nb\r\n");

// An unnumbered line is a direct command, as if typed.
r = script('print "direct"\n10 print "prog"\n');
is("a direct command", r.out, "direct\r\nprog\r\n");

// INPUT reads stdin, not the rest of the file.
r = script('10 input "Guess"; g\n20 print "got"; g\n', "42\n");
is("INPUT reads stdin", r.out, "Guess? got 42 \r\n");

// An error is reported on a line of its own and sets the status, and a later
// line still runs -- the file is a session until its end.
r = script('total = 1\nprint "after"\n10 print "prog"\n');
is("an error mid-file", r.out, "\r\n?Syntax error\r\nafter\r\nprog\r\n");
is("the error status", r.status, 1);

// An error inside the implicit run ends it.
r = script("10 print 1/0\n20 print \"never\"\n");
is("an error while running", r.out, "\r\n?Division by zero error in 10\r\n");
is("its status", r.status, 1);

// A file with nothing to run is not an error.
r = script('print "just a command"\n');
is("no program at all", r.out, "just a command\r\n");
is("its status", r.status, 0);

// A missing file says so and exits 1, writing nothing to stdout.
{
    let now = 90000;
    H.type("mb /tmp/nosuch.bas >/tmp/n");
    H.press(H.KEY.ENTER);
    for (let d = H.run(now); d !== -1; d = H.run(now))
        now += d > 0 ? d : 1;
    const s = H.screen();
    if (H.row(s, s.cursor_y) !== H.prompt(1))
        die(`a missing file did not exit 1: ${JSON.stringify(H.row(s, s.cursor_y))}`);
    if (!H.rows(s).join("\n").includes("cannot read"))
        die("a missing file printed no diagnostic");
}

// A bare name is looked for among the shipped examples, as LOAD does. The scan
// matches the `mbasic-' prefix, so the version here is arbitrary.
{
    const STORE = "/pkg/store/mbasic-9.9-r9";
    for (const d of ["/pkg", "/pkg/store", STORE, `${STORE}/share`])
        H.store.dirs.add(d);
    H.store.files.set(`${STORE}/share/greet.bas`,
        new TextEncoder().encode('10 print "from the store"\n'));
    let now = 91000;
    H.type("mb greet.bas >/tmp/g");
    H.press(H.KEY.ENTER);
    for (let d = H.run(now); d !== -1; d = H.run(now))
        now += d > 0 ? d : 1;
    const got = new TextDecoder().decode(H.store.files.get("/tmp/g") ?? new Uint8Array());
    is("a bare name from the store", got, "from the store\r\n");
}

// One long transcript, to show what a shipped example looks like run this way.
golden("script.log", script(
    '10 rem A worked example\n' +
    '20 for i = 1 to 3\n' +
    '30   print "line"; i\n' +
    '40 next i\n' +
    '50 input "Name"; n$\n' +
    '60 print "Hello, "; n$\n', "World\n").out);

ok();
