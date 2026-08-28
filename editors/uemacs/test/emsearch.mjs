// Searching and replacing, including the incremental search -- which is the
// one command that reads a key, decides it was not part of the pattern, and
// hands it back for the command loop to find.

import { boot, em, put, press, keys, screen, cursor, is, ok, tick, H } from "./emlib.mjs";

await boot("emsearch");

const FILE = "alpha beta\ngamma delta\nalpha omega\nbeta gamma\n";

// The search prompts end with <Meta> rather than a return -- the echo line
// says so -- because a return is a character a pattern may contain.
//
// C-s searches forward and leaves the dot after the match.
put("/tmp/f", FILE);
em("/tmp/f");
keys("^s");
H.type("delta");
keys("ESC");
tick(1);
is("C-s", cursor(), "11,1");

// C-r searches back.
keys("^r");
H.type("beta");
keys("ESC");
tick(1);
is("C-r", cursor(), "6,0");

// A search that fails says so and leaves the dot alone.
em("/tmp/f");
keys("^s");
H.type("nowhere");
keys("ESC");
tick(1);
is("a failed search", screen().split("\n").pop(), "Not found");
is("and the dot stays", cursor(), "0,0");

// C-x S is the incremental search: each key narrows it, and the key that ends
// it is handed back rather than eaten -- C-a here, which goes to column 0.
em("/tmp/f");
keys("^x", "S");
for (const c of "omega") {
    H.type(c);
    tick(1);
}
is("C-x S while typing", cursor(), "11,2");
keys("^a");
tick(1);
is("the key that ended it ran", cursor(), "0,2");

// M-r replaces every match from the dot on, and says how many.
em("/tmp/f", ["ESC", "r"]);
H.type("gamma");
keys("ESC");
H.type("GAMMA");
keys("ESC");
tick(1);
is("M-r", screen(4), "alpha beta\nGAMMA delta\nalpha omega\nbeta GAMMA");
is("and counts them", screen().split("\n").pop(), "2 substitutions");

// MAGIC mode, which emacs.rc does not turn on, so the pattern is literal
// until it is asked for.
em("/tmp/f");
keys("ESC", "x");
H.type("add-mode");
keys("CR");
H.type("magic");
keys("CR");
tick(1);
keys("^s");
H.type("a[lm]pha");
keys("ESC");
tick(1);
is("a regexp under MAGIC", cursor(), "5,0");

ok("forward, back, incremental, replace and MAGIC");
