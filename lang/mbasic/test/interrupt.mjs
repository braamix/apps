// ^C, on the grid rather than through a pipe: a pipe has no keyboard.
//
// Upstream's ISCNTC polled the keyboard once per statement, and the Apple
// version of it was broken -- it compared a value INCHR had already masked
// with ANDI 127 against the unmasked 131, so the comparison always failed and
// the ^C was eaten without interrupting (13-porting-notes.md §3.1). That is
// one of the ten defects this port fixes.
//
// Two things have to hold for it to work here. A signal is delivered where a
// process parks, so the statement burst has to park; and a key ring has one
// receiver, so the keyboard goes back to the console the moment a program runs
// long enough to yield -- which is exactly when the console's pump, not the
// line editor, is the thing that should see a ^C.

import { boot, die, ok } from "./mblib.mjs";

const H = await boot("interrupt");

let now = 100;
const line = (t) => H.submit(t, now++);
const flat = (s) => H.rows(s).join("\n");

line("mb");
line(""); // MEMORY SIZE?
line(""); // TERMINAL WIDTH?
if (!flat(H.screen()).includes("Braam BASIC v1.1"))
    die("the banner did not appear");

line("10 PRINT 1;:GOTO 10");

// Start it, THEN interrupt: batching the ^C behind the Enter would put both in
// the console pump's hands at once and the interrupt would arrive before the
// RUN it was meant to stop.
H.type("RUN");
H.press(H.KEY.ENTER);
H.run(now++);
H.press("c".codePointAt(0), H.CTRL);
for (let i = 0; i < 60 && H.run(now++) !== -1; i++)
    ;

let s = H.screen();
if (!flat(s).includes("Break in 10"))
    die(`^C did not break the program:\n${flat(s)}`);

// STOP and ^C are continuable; an error is not. The frames survive too.
line("PRINT \"ALIVE\"");
if (!flat(H.screen()).includes("ALIVE"))
    die("the interpreter did not read again after the break");

// ^D on an empty line is end of input. Upstream had no way to leave BASIC at
// all -- it owned the machine -- and the keyboard has to go back either way,
// or the shell never reads again.
H.press("d".codePointAt(0), H.CTRL);
for (let i = 0; i < 20 && H.run(now++) !== -1; i++)
    ;
s = H.submit("echo back", now++);
if (!H.rows(s).includes("back"))
    die(`the shell did not get its keyboard back:\n${flat(s)}`);
if (H.row(s, s.cursor_y) !== H.prompt())
    die("the shell's prompt did not come back at status 0");

ok();
