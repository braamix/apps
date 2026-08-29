// The screen: hex mode, the pull-down menu, and a resize.
//
// The menu is also the proof that mainmenu parsed -- it is read at startup
// through the scanners that went into the SDK for it.

import { boot, le, put, keys, press, screen, status, regrid, tick, is, ok, H } from "./lelib.mjs";

await boot("lescreen");
put("/tmp/f", "alpha\nbeta\ngamma\ndelta\n");
le("/tmp/f");

// Hex mode: the offset, sixteen bytes, and the same bytes as text. A control
// character shows as the letter it is a control of, which is upstream's own
// visualisation.
keys("^a", "h");
tick(2);
is("hex mode", screen(3),
`00000000   61 6C 70 68 61 0A 62 65 74 61 0A 67 61 6D 6D 61   alphaJbetaJgamma
00000010   0A 64 65 6C 74 61 0A                              JdeltaJ
00000017`);
is("and the status counts in octal", status().slice(0, 13), "OctOffs:00   ");

keys("^a", "h");
tick(2);
is("and back to text", screen(1), "alpha");

// The menu bar, from the mainmenu file the package ships.
press("^n");
tick(2);
is("the menu bar", screen(1),
   "   File  Edit  Block  Search  Move  Format  Others  Options  Help");
/* One is enough: ESC has a code of its own and is no longer a prefix. */
press("ESC");
tick(2);
is("and it closes again", screen(1), "alpha");

// A resize: the geometry rides on the key reply, next_key reshapes the grid
// before it reports, and the editor lays itself out again.
regrid(60, 16);
tick(2);
is("the text after a resize", screen(4),
`alpha
beta
gamma
delta`);
is("the grid is the new shape", `${H.screen().cols}x${H.rows(H.screen()).length}`, "60x16");
is("and the status line moved with it", status().slice(0, 16), "Line=1     Col=1");

ok("hex mode, the menu and a resize");
