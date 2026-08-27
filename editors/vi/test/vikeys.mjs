// Visual mode: the screen it paints and the motions that move around it.
//
// Keys in, cells out. The screen is asserted whole, which is what makes this
// worth having -- it is the only test that says vtube reached the Grid at all.

import { boot, vi, press, screen, fg, cursor, put, is, ok } from "./exlib.mjs";

await boot("vikeys");

const FILE = "alpha\nbeta\ngamma\ndelta\n";
const TILDE = "~\n".repeat(19).replace(/\n$/, "");

// Entering visual: the buffer at the top, ~ for the rows past the end, and the
// file's name on the last line. That last line is row 23 of 24, so the ~ run
// is nineteen rows.
put("/tmp/f", FILE);
is("the first frame", vi("/tmp/f"),
`alpha
beta
gamma
delta
${TILDE}
"/tmp/f" 4 lines, 23 characters`);

// Three colours in that frame: the text, the ~ rows and the echo line.
is("the colours", [fg(0), fg(4), fg(23)].join(" "), "white blue cyan");

// x deletes the character under the cursor, and the cursor started at the top
// left; the rest of the screen must not move.
press("j");
press("j");
press("x");
is("x on the third line", screen(5),
`alpha
beta
amma
delta
~`);

// A count applies to the motion, not the character.
put("/tmp/f", FILE);
vi("/tmp/f", ["3", "x"]);
is("3x", screen(2), "ha\nbeta");

// hjkl, the motions the arrow keys are named after, and a count on each.
put("/tmp/f", FILE);
vi("/tmp/f", ["j", "j", "l", "l", "r", "Z"]);
is("jjll", screen(3), "alpha\nbeta\ngaZma");

put("/tmp/f", FILE);
vi("/tmp/f", ["3", "j", "2", "l", "k", "h", "r", "Z"]);
is("3j 2l k h", screen(3), "alpha\nbeta\ngZmma");

// A motion changes no character, so the only thing that says it happened is
// where the cursor is -- and the cursor reaches the screen in the header of a
// blit that a frame with nothing damaged in it never sends.
put("/tmp/f", FILE);
vi("/tmp/f");
is("the cursor starts home", cursor(), "0,0");
for (const [k, at] of [["l", "1,0"], ["l", "2,0"], ["j", "2,1"], ["k", "2,0"],
                       ["h", "1,0"], ["$", "4,0"], ["0", "0,0"], ["G", "0,3"],
                       ["UP", "0,2"], ["RIGHT", "1,2"], ["w", "0,3"]]) {
    press(k);
    is(`the cursor after ${k}`, cursor(), at);
}

// Word motions, and r to prove where the cursor ended up.
put("/tmp/f", FILE);
vi("/tmp/f", ["w", "r", "Q"]);
is("w crosses the line end", screen(3), "alpha\nQeta\ngamma");

// $ and ^, and the arrow keys, which have no escape sequence to be told apart
// from an escape: a named key arrives whole.
put("/tmp/f", FILE);
vi("/tmp/f", ["$", "r", "Z"]);
is("$", screen(1), "alphZ");

put("/tmp/f", FILE);
vi("/tmp/f", ["DOWN", "DOWN", "RIGHT", "r", "Z"]);
is("arrows", screen(3), "alpha\nbeta\ngZmma");

// ^F pages forward. A file of forty lines and a screen of twenty-three shows
// line 22 at the top afterwards -- vi keeps two lines of overlap.
put("/tmp/f", Array.from({ length: 40 }, (_, i) => `line${i + 1}`).join("\n") + "\n");
vi("/tmp/f", ["^F"]);
is("^F", screen(3), "line22\nline23\nline24");

// ^C abandons whatever is half-typed and leaves the editor where it was.
put("/tmp/f", FILE);
vi("/tmp/f", ["2", "^C", "d", "d"]);
is("^C abandons a count", screen(3), "beta\ngamma\ndelta");

ok("the first frame, x, counts, hjkl, word and arrow motions, ^F and ^C");
