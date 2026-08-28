// The screen it paints and the keys that move around it.
//
// Keys in, cells out. The first frame is asserted whole, which is what makes
// this worth having: it is the only case that says the back buffer reached the
// Grid at all.

import { boot, em, put, press, keys, screen, cursor, fg, attrs, ATTR_REVERSE, is, ok }
    from "./emlib.mjs";

await boot("emkeys");

const FILE = "alpha\nbeta\ngamma\ndelta\n";
const BLANK = "\n".repeat(18).replace(/\n$/, "");

// Entering: the buffer at the top, blank rows below it, and the mode line on
// row 22 of 24. The message line under it is the twenty-fourth.
put("/tmp/f", FILE);
is("the first frame", em("/tmp/f"),
`alpha
beta
gamma
delta
${BLANK}
-- uEmacs/Pk 4.0.15: f (Spell utf-8) /tmp/f ----------------------------- All --`);

// Three things the frame says that the text does not: the mode line is in
// reverse video, the message line is cyan, and the text is white.
is("the mode line stands out", String(attrs(22)), String(ATTR_REVERSE));
is("the colours", [fg(0), fg(23)].join(" "), "white cyan");

// The cursor starts at the top left and reaches the screen in the header of a
// blit -- a motion damages no cell, so this is the only thing that says it
// happened.
is("the cursor starts home", cursor(), "0,0");

// C-n, C-p, C-f, C-b, C-a, C-e: the motions the arrow keys are named after.
keys("^n", "^n", "^e");
is("C-n C-n C-e", cursor(), "5,2");
keys("^a");
is("C-a", cursor(), "0,2");
keys("^f", "^f");
is("C-f C-f", cursor(), "2,2");
keys("^b");
is("C-b", cursor(), "1,2");
keys("^p");
is("C-p", cursor(), "1,1");

// The arrow keys reach the same commands. Upstream decoded ESC [ A out of the
// byte stream; a named key arrives whole and screen.cpp maps it to SPEC|'A',
// which is what ebind.c has always bound.
em("/tmp/f", ["DOWN", "RIGHT", "RIGHT"]);
is("the arrows", cursor(), "2,1");
keys("UP", "LEFT");
is("up and left", cursor(), "1,0");

// Home, End and Delete. They are not the VT220 keypad the SPEC number block
// belongs to, so a wrong mapping is loud: End would set the mark and Delete
// would kill a region.
em("/tmp/f", ["^n", "^f", "END"]);
is("End", cursor(), "4,1");
keys("HOME");
is("Home", cursor(), "0,1");
keys("DELETE");
is("Delete", screen(3), "alpha\neta\ngamma");

// Control on a named key is the word and buffer motions, which key_code()
// hands over as Meta keys.
em("/tmp/f", ["^n", "^n", "^LEFT"]);
is("C-Left is a word back", cursor(), "0,1");
keys("^RIGHT");
is("C-Right is a word on", cursor(), "0,2");
keys("^END");
is("C-End is the end of the buffer", cursor(), "0,4");
keys("^HOME");
is("C-Home is the start of it", cursor(), "0,0");

// M-< and M-> are the ends of the buffer.
keys("ESC", "<");
is("M-<", cursor(), "0,0");
keys("ESC", ">");
is("M->", cursor(), "0,4");

// A count: C-u takes digits, and the command runs that many times.
em("/tmp/f", ["^u", "3", "^n"]);
is("C-u 3 C-n", cursor(), "0,3");

// C-u on its own is four.
em("/tmp/f", ["^u", "^n"]);
is("C-u C-n", cursor(), "0,4");

// M-f and M-b step by words, not characters: from the start of "gamma", back
// one word is the start of "beta" on the line above.
em("/tmp/f", ["^n", "^n", "ESC", "b"]);
is("M-b", cursor(), "0,1");
// M-f leaves the dot after the word it crossed, which for "beta" is the start
// of the line below.
keys("ESC", "f");
is("M-f", cursor(), "0,2");

ok("the frame, its colours, and the motions");
