// The blocks and the undo, which are what LE is for.

import { boot, le, put, keys, press, screen, status, tick, is, ok } from "./lelib.mjs";

await boot("leblock");
put("/tmp/f", "alpha\nbeta\ngamma\ndelta\n");
le("/tmp/f");

// A stream block: F5 marks the start, F6 the end, F11 copies it here.
press("F5");
keys("DOWN");
press("F6");
keys("DOWN", "DOWN", "DOWN");
press("F11");
tick(2);
is("a stream block copies", screen(6),
`alpha
beta
gamma
delta
alpha`);
is("and the size grew by the block", status().slice(20, 28), "Sz:29   ");

// Undo takes the whole insertion back, and redo puts it in again.
press("^u");
tick(1);
is("undo drops it", screen(6),
`alpha
beta
gamma
delta`);
press("M-z");
tick(1);
is("redo brings it back", screen(6),
`alpha
beta
gamma
delta
alpha`);

// Typing, then undo: the change-gluing makes one group of a run of inserts.
press("^u");
tick(1);
keys("^g", "b");            // beginning-of-file: the undo left the cursor low
keys("X", "Y", "Z");
is("three characters", screen(1), "XYZalpha");
press("^u");
tick(1);
is("undo takes the run", screen(1), "alpha");

// Marking by keyboard. F4 H hides the old block first, so pre_mark_move starts
// a fresh one at the cursor.
keys("^g", "b");
keys("F4", "h");
press("S-DOWN");
press("S-DOWN");
press("F11");
tick(2);
is("Shift+Down marks a line at a time", screen(7),
`alpha
beta
alpha
beta
gamma
delta`);

// Ctrl+Down is unbound: it used to mark, which made the vertical pair the odd
// one out. With the block hidden there is nothing for F11 to copy.
press("^u");
tick(1);
keys("^g", "b");
keys("F4", "h");
press("^DOWN");
press("^DOWN");
press("F11");
tick(2);
is("Ctrl+Down does not mark", screen(5),
`alpha
beta
gamma
delta`);

ok("stream blocks, undo, redo and marking by keyboard");
