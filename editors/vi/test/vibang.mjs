// [Hit return to continue]: the pause that keeps a : command's output.
//
// Two shapes. A shell escape hands the screen back, so the child writes on the
// scrolling console and the pause happens with the claim released; a :p writes
// through vi's own output buffer onto the same cells, with both claims held.
// Both are answered by one key, and the repaint that erases the output waits
// for it.

import { boot, vi, press, tick, screen, put, quitvi, is, ok } from "./exlib.mjs";

await boot("vibang");

const FILE = "alpha\nbeta\ngamma\n";
const BUF = "alpha\nbeta\ngamma\n~\n~";
const HITRET = "[Hit return to continue]";

// The last four rows: what a pause leaves at the bottom of the screen.
const tail = () => screen().split("\n").slice(-3).join("\n");

// :! -- the child's output, then the prompt under it. Spawning a child and
// reaping it takes more ticks than the one press() runs.
put("/tmp/f", FILE);
vi("/tmp/f", [":", "!echo hello", "CR"]);
tick(6);
is(":! pauses under its output", tail(), `home $ vi /tmp/f\nhello\n${HITRET}`);

// Any key answers it, and the buffer comes back.
press(" ");
tick(2);
is("a key returns to the buffer", screen(5), BUF);

// :p -- the same pause with both claims held. The printed lines scroll up from
// the echo line, which is what upstream's did on a real terminal.
press(":");
press("1,2p");
press("CR");
tick(3);
is(":p pauses too", tail(), `alpha\nbeta\n${HITRET}`);
press(" ");
tick(2);
is("and repaints after it", screen(5), BUF);

// : at the prompt opens the next : line rather than being eaten, and the
// command it runs pauses in its turn.
press(":");
press("!echo one");
press("CR");
tick(6);
press(":");
tick(2);
is(": at the prompt starts a command", screen().split("\n").pop(), ":");
press("1p");
press("CR");
tick(3);
is("and that command pauses in its turn", tail(), `:1p\nalpha\n${HITRET}`);
press(" ");
tick(2);
is("still the same buffer", screen(5), BUF);

quitvi();
ok(":! and :p pause, a key resumes, and : at the prompt chains");
