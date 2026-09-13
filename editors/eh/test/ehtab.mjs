// A TAB in the last eight columns of a row.
//
// Not upstream's: cases.json is its suite and nothing there gets past column
// 56. The writer used to blank to the next eight-column stop over a column that
// clamped at COLS-1, so a tab starting anywhere in columns 72-79 of an
// 80-column screen never reached its stop and display() spun for ever.
//
// A regression here *wedges* this suite rather than failing it: the harness
// pumps until the kernel is idle, with no fuel limit and no timeout.

import { boot, put, rm, tick, submit, chdir, is, ok, image } from "./ehlib.mjs";

await boot("ehtab");
submit("cd /tmp");
tick(2);
chdir("/tmp");

// Nine tabs land the tenth at column 72, the first column that used to hang.
rm("/tmp/tab.txt");
put("/tmp/tab.txt", "\t".repeat(10) + "x\n");
submit("eh tab.txt");
tick(3);

const rows = image().split("\n---\n")[0].split("\n");

// Row 0 is the status line; the text is row 1. The ten tabs fill 0..79 and the
// row is blank -- the last stop is clipped to the edge, so the x wraps.
is("the tab row is blank to the edge", rows[1], " ".repeat(80));
is("the x follows on the next row", rows[2].trimEnd(), "x");

ok("a TAB in the last eight columns of a row");
