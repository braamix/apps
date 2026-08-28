// Windows, and a resize in the middle of a session.
//
// The resize is what says the geometry still arrives: upstream took it from a
// SIGWINCH handler and an ioctl, and it rides on the key read here.

import { boot, em, put, press, keys, screen, row, modeline, regrid, is, ok, tick, H }
    from "./emlib.mjs";

await boot("emwindow");

put("/tmp/one", "first\nsecond\nthird\n");
put("/tmp/two", "alpha\nbeta\ngamma\n");

// One window fills the screen: rows 0..21 of text, the mode line on 22 and
// the message line on 23.
em("/tmp/one");
is("one window", row(0) + " / " + modeline().slice(0, 24),
   "first / -- uEmacs/Pk 4.0.15: one");

// C-x 2 splits it in two. Each half keeps a mode line of its own, so the
// upper one ends on row 10 and the lower begins on row 11.
keys("^x", "2");
is("the upper window", [row(0), row(1), row(2)].join("\n"), "first\nsecond\nthird");
is("the split's mode line", /uEmacs/.test(row(10)) ? "modeline" : row(10), "modeline");
is("the lower window shows the same buffer", row(11), "first");

// C-x o moves to the other window, and C-x C-f there changes only that half.
keys("^x", "o", "^x", "^f");
H.type("/tmp/two");
keys("CR");
tick(1);
is("two buffers at once", [row(0), row(11)].join(" / "), "first / alpha");

// C-x 1 gives the screen back to the window the dot is in.
keys("^x", "1");
is("C-x 1", row(0) + " / " + row(11), "alpha / ");

// C-x z and C-x C-z grow and shrink; with one window there is nothing to
// take the rows from and it says so.
keys("^x", "z");
is("nothing to grow into", screen().split("\n").pop(), "Only one window");

// A resize mid-session: the buffer stays, and the mode line follows the new
// width. 100x30 leaves 28 rows of text, the mode line on 28 and the message
// line on 29.
em("/tmp/one");
keys("^n");
regrid(100, 30);
is("the buffer survived a resize", [row(0), row(1), row(2)].join("\n"),
   "first\nsecond\nthird");
is("the mode line moved down and grew", String(row(28).length), "100");
is("and the cursor is where it was", H.screen().cursor_y + "", "1");

// And back to a smaller one.
regrid(60, 20);
is("and back", [row(0), row(1)].join("\n"), "first\nsecond");
is("the mode line again", String(row(18).length), "60");

// A drag is a burst of resizes, and the process sees none of them until it is
// next scheduled -- so the geometry it ends up with can be the one it already
// had. The kernel blanks its screen for each of them anyway, keeping only the
// rows above the cursor, so a frame sent by difference would send nothing and
// leave the screen black. H.regrid() directly, since regrid() ticks.
H.regrid(100, 30, "resize returned no screen descriptor");
H.regrid(60, 20, "resize returned no screen descriptor");
tick(3);
is("a burst of resizes ending where it started", [row(0), row(1)].join("\n"),
   "first\nsecond");
is("and the mode line with it", String(row(18).length), "60");

ok("split, switch, close, and a resize either way");
