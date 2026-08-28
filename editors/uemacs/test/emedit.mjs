// Changing the text, and getting it back out again.
//
// The file written at the end is what makes this more than a screen test: it
// says the buffer, the display and fileio.cpp all agree about what is there.

import { boot, em, put, get, press, keys, screen, is, ok, tick, H } from "./emlib.mjs";

await boot("emedit");

const FILE = "alpha\nbeta\ngamma\ndelta\n";

// Self-insert, which is every key that is not bound.
put("/tmp/f", FILE);
em("/tmp/f", ["^n", "^e"]);
H.type("!!");
tick(1);
is("typing", screen(3), "alpha\nbeta!!\ngamma");

// C-d takes the character under the dot, C-h the one before it.
em("/tmp/f", ["^n", "^f", "^d"]);
is("C-d", screen(3), "alpha\nbta\ngamma");
keys("^h");
is("C-h", screen(3), "alpha\nta\ngamma");

// C-o opens a line without moving off it; C-k takes the rest of the line, and
// a second C-k the newline it left behind.
em("/tmp/f", ["^n", "^o"]);
is("C-o", screen(4), "alpha\n\nbeta\ngamma");
em("/tmp/f", ["^n", "^k"]);
is("C-k", screen(4), "alpha\n\ngamma\ndelta");
keys("^k");
is("C-k again", screen(3), "alpha\ngamma\ndelta");

// The kill buffer: what C-k took, C-y puts back. Twice over with an argument.
em("/tmp/f", ["^n", "^k", "^k", "^n", "^y"]);
is("C-y", screen(5), "alpha\ngamma\nbeta\ndelta\n");

// A region: M-space sets the mark -- this fork's binding, where upstream's
// was C-@ -- then a motion, and C-w takes what is between them.
em("/tmp/f", ["ESC", " ", "^n", "^n", "^w"]);
is("C-w over two lines", screen(3), "gamma\ndelta\n");
keys("ESC", ">", "^y");
is("and C-y at the end", screen(4), "gamma\ndelta\nalpha\nbeta");

// M-w copies rather than takes.
em("/tmp/f", ["ESC", " ", "^n", "ESC", "w", "ESC", ">", "^y"]);
is("M-w leaves the region alone", screen(5), "alpha\nbeta\ngamma\ndelta\nalpha");

// Case: M-u, M-l and M-c on a word.
em("/tmp/f", ["ESC", "u"]);
is("M-u", screen(2), "ALPHA\nbeta");
// Each of them leaves the dot after the word, so M-b comes first.
em("/tmp/f", ["ESC", "u", "ESC", "b", "ESC", "l"]);
is("M-l puts it back", screen(2), "alpha\nbeta");
em("/tmp/f", ["ESC", "c"]);
is("M-c", screen(2), "Alpha\nbeta");

// The report line, and the file C-x C-s writes.
put("/tmp/f", FILE);
em("/tmp/f", ["^n", "^e"]);
H.type("!");
keys("^x", "^s");
tick(2);
is("the report", screen().split("\n").pop(), "(Wrote 4 lines)");
is("the file", get("/tmp/f"), "alpha\nbeta!\ngamma\ndelta\n");

// C-x C-w writes somewhere else and the buffer follows the name.
em("/tmp/f", ["^n", "^e"]);
H.type("?");
keys("^x", "^w");
H.type("/tmp/g");
keys("CR");
tick(2);
is("C-x C-w", get("/tmp/g"), "alpha\nbeta!?\ngamma\ndelta\n");
is("the old file is untouched", get("/tmp/f"), "alpha\nbeta!\ngamma\ndelta\n");

ok("insert, delete, kill, yank, case and the two writes");
