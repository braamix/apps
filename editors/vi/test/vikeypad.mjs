// The keys that are not characters: the arrows, Home, End, Delete, Backspace
// and escape.
//
// Upstream read them as termcap strings and mapped them into ordinary
// commands, which is all command mode needs. Insert mode needs more than a
// byte -- a space is a space and an arrow is not -- and this is what says so:
// every case here presses a real named key and reads the line it left.

import { boot, vi, press, screen, cursor, fg, put, quitvi, is, ok } from "./exlib.mjs";

await boot("vikeypad");

const FILE = "alpha\nbeta\ngamma\ndelta\n";

function edit(keys, want, what) {
    put("/tmp/f", FILE);
    vi("/tmp/f", keys);
    is(what, screen(4), want);
}

/* ------------------------------------------------------- command mode */

edit(["DOWN", "DOWN", "RIGHT", "x"], "alpha\nbeta\ngmma\ndelta", "down down right");
edit(["DOWN", "END", "x"], "alpha\nbet\ngamma\ndelta", "end");
edit(["l", "l", "LEFT", "x"], "apha\nbeta\ngamma\ndelta", "left");
edit(["DELETE"], "lpha\nbeta\ngamma\ndelta", "delete");
edit(["BACKSPACE", "x"], "lpha\nbeta\ngamma\ndelta", "backspace at column 0");
edit(["3", "DOWN", "x"], "alpha\nbeta\ngamma\nelta", "a count applies");

// Home is ^, the first non-blank, so the indent is not where it lands.
put("/tmp/f", "    indented\n");
vi("/tmp/f", ["END", "HOME", "x"]);
is("home", screen(1), "    ndented");

// An operator takes a cursor key as its motion, not as a cancel.
edit(["l", "d", "END"], "a\nbeta\ngamma\ndelta", "d End");
edit(["l", "l", "d", "LEFT"], "apha\nbeta\ngamma\ndelta", "d Left");
edit(["d", "DOWN"], "gamma\ndelta\n~\n~", "d Down");

// A modifier does not swallow the key.
edit(["^DOWN", "x"], "alpha\neta\ngamma\ndelta", "ctrl down");

/* -------------------------------------------------------- insert mode */

// The caret moves and the insertion goes on: what is typed next lands where
// the key left it, which is the whole point.
edit(["i", "XY", "LEFT", "1", "ESC"], "X1Yalpha\nbeta\ngamma\ndelta", "left while inserting");
edit(["i", "XY", "RIGHT", "1", "ESC"], "XYa1lpha\nbeta\ngamma\ndelta", "right");
edit(["i", "XY", "HOME", "1", "ESC"], "1XYalpha\nbeta\ngamma\ndelta", "home");
edit(["i", "XY", "END", "1", "ESC"], "XYalpha1\nbeta\ngamma\ndelta", "end");
edit(["i", "XY", "DOWN", "1", "ESC"], "XYalpha\nbe1ta\ngamma\ndelta", "down keeps the column");
edit(["j", "i", "X", "UP", "1", "ESC"], "a1lpha\nXbeta\ngamma\ndelta", "up");

// At the ends of the line there is no character to move onto, which is where
// the two ideas of a cursor -- on a character, and between two -- differ.
edit(["A", "Z", "LEFT", "LEFT", "1", "ESC"], "alph1aZ\nbeta\ngamma\ndelta", "left from the end");
edit(["A", "Z", "LEFT", "RIGHT", "1", "ESC"], "alphaZ1\nbeta\ngamma\ndelta", "right to the end");
edit(["A", "Z", "RIGHT", "1", "ESC"], "alphaZ1\nbeta\ngamma\ndelta", "right at the end beeps");
edit(["i", "LEFT", "1", "ESC"], "1alpha\nbeta\ngamma\ndelta", "left at column 0 beeps");

// Delete takes the character under the caret; Backspace still rubs out.
edit(["i", "XY", "DELETE", "1", "ESC"], "XY1lpha\nbeta\ngamma\ndelta", "delete while inserting");
edit(["i", "XY", "DELETE", "DELETE", "1", "ESC"], "XY1pha\nbeta\ngamma\ndelta", "delete twice");
edit(["A", "Z", "DELETE", "1", "ESC"], "alphaZ1\nbeta\ngamma\ndelta", "delete at the end beeps");
edit(["i", "abc", "BACKSPACE", "1", "ESC"], "ab1alpha\nbeta\ngamma\ndelta", "backspace");

// And past the start of the insertion it takes the line itself, which is a
// port addition: upstream stopped at what this insertion had typed.
edit(["i", "BACKSPACE", "1", "ESC"], "1alpha\nbeta\ngamma\ndelta", "backspace at column 0 beeps");
edit(["l", "l", "i", "BACKSPACE", "1", "ESC"], "a1pha\nbeta\ngamma\ndelta", "past the start");
edit(["l", "l", "i", "BACKSPACE", "BACKSPACE", "1", "ESC"], "1pha\nbeta\ngamma\ndelta", "twice");
edit(["l", "l", "i", "X", "BACKSPACE", "BACKSPACE", "1", "ESC"], "a1pha\nbeta\ngamma\ndelta",
     "through the insertion and out the other side");
edit(["l", "l", "i", "^h", "1", "ESC"], "a1pha\nbeta\ngamma\ndelta", "^H is the same key");

// After A the caret sits on the terminator, which is not a character: the
// rub-out has to work there too, and leave the insertion at the end.
edit(["A", "BACKSPACE", "1", "ESC"], "alph1\nbeta\ngamma\ndelta", "at the end of the line");
edit(["A", "BACKSPACE", "BACKSPACE", "1", "ESC"], "alp1\nbeta\ngamma\ndelta", "twice at the end");

// The autoindent is below the start of the insertion and goes the same way.
put("/tmp/f", "    indented\n");
vi("/tmp/f", [":", "se ai", "CR", "o", "BACKSPACE", "X", "ESC"]);
is("backspace over the autoindent", screen(2), "    indented\n   X");

// A page leaves the insertion, as an escape would: there is no window to
// insert into two screens away.
put("/tmp/f", Array.from({ length: 60 }, (_, i) => `l${i + 1}`).join("\n") + "\n");
vi("/tmp/f", ["i", "X", "PAGE_DOWN"]);
is("page down leaves insert", screen(1), "l22");

// ^V quotes the key that follows, so an arrow can still be typed as its byte.
edit(["i", "^v", "UP", "ESC"], "^Palpha\nbeta\ngamma\ndelta", "quoted arrow");

/* --------------------------------------------------------- the mode line */

// The echo area says so while an insertion is open, in bright white, and has
// itself back when it closes. Row 23 of 24, and the whole of it.
const MODE = "-- INSERT --";

function mode(what, keys, want, colour = "white+") {
    put("/tmp/f", FILE);
    vi("/tmp/f", keys);
    is(what, screen().split("\n")[23] ?? "", want);
    is(`${what}, in colour`, fg(23), colour);
}

mode("i, before a character is typed", ["i"], MODE);
mode("and while typing", ["i", "XY"], MODE);
mode("A", ["A"], MODE);
mode("o", ["o"], MODE);
mode("R", ["R"], MODE);
mode("c", ["c", "w"], MODE);
mode("across a motion", ["i", "X", "LEFT"], MODE);
mode("across a rub-out", ["l", "i", "X", "BACKSPACE", "BACKSPACE"], MODE);
mode("gone after escape", ["i", "XY", "ESC"], `"/tmp/f" 4 lines, 23 characters`, "cyan");

// r takes one character and is not a mode; nor is the : line.
mode("not r", ["r"], `"/tmp/f" 4 lines, 23 characters`, "cyan");
mode("not the : line", [":", "se"], ":se", "cyan");
press("ESC");

/* ------------------------------------------------------- the : line */

// An arrow on the command line is discarded -- not inserted, and not a
// finished line: the command still runs.
put("/tmp/f", FILE);
vi("/tmp/f", [":", "se", "LEFT", "t nu", "CR"]);
is("an arrow on the : line", screen(1), "     1  alpha");

// Backspacing off the ':' prompt abandons the command line, and does not rub
// out the buffer under it: the echo area has its own floor.
put("/tmp/f", FILE);
vi("/tmp/f", [":", "se", "BACKSPACE", "BACKSPACE", "BACKSPACE", "x"]);
is("backspace off the : prompt", screen(4), "lpha\nbeta\ngamma\ndelta");

/* --------------------------------------------------- escape and undo */

// Escape ends the insertion; a second one only beeps.
edit(["i", "XY", "ESC", "ESC", "x"], "Xalpha\nbeta\ngamma\ndelta", "escape twice");

// And the cursor it leaves is on the screen, not only in the editor: escape
// steps back one, and that is a frame with no character changed in it.
put("/tmp/f", FILE);
vi("/tmp/f", ["i", "bar"]);
is("the caret while inserting", cursor(), "3,0");
press("ESC");
is("the caret after escape", cursor(), "2,0");
press("h");
is("and a motion after that", cursor(), "1,0");

// u undoes the segment the last motion began, U the whole line.
edit(["i", "ab", "LEFT", "c", "ESC", "u"], "abalpha\nbeta\ngamma\ndelta", "u after a motion");
edit(["i", "ab", "LEFT", "c", "ESC", "u", "U"], "alpha\nbeta\ngamma\ndelta", "U after a motion");

quitvi();

ok("the arrows, Home, End, Delete, Backspace and escape, in both modes, and the mode line");
