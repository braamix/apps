// Load a file, move about, change it, save it, and read it back out of the
// store. The round trip is what proves the gap buffer, the key tree, the
// action table and the file I/O are all wired to each other.

import { boot, le, put, get, keys, press, screen, status, cursor, tick, is, ok } from "./lelib.mjs";

await boot("leedit");

const FILE = "alpha\nbeta\ngamma\ndelta\n";
put("/tmp/f", FILE);

le("/tmp/f");
is("the first frame", screen(4),
`alpha
beta
gamma
delta`);

is("the status line", status(),
   'Line=1     Col=1    Sz:23     Ch:97    IA   "f" Offs:0 (0%)');
is("the cursor starts home", cursor(), "0,0");

// The arrow keys reach the action table, which is the whole of the keymap.
keys("DOWN", "DOWN", "END");
is("Down Down End", cursor(), "5,2");
is("and the status agrees", status().slice(0, 16), "Line=3     Col=6");

keys("!");
is("a typed character lands", screen(4),
`alpha
beta
gamma!
delta`);
is("and marks the file modified", status().includes("*IA") ? "yes" : "no", "yes");

press("F2");
tick(4);
is("F2 saves", get("/tmp/f"), "alpha\nbeta\ngamma!\ndelta\n");
is("and clears the modified mark", status().includes("*IA") ? "yes" : "no", "no");

ok("load, edit and save round trip");
