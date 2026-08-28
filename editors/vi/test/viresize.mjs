// A resize under vi.
//
// The geometry rides on every terminal reply, so the grid is already the new
// shape when SIG_WINCH arrives; what this asserts is the other half -- that vi
// re-cut vtube at the new width and repainted, without losing the buffer or
// where the cursor was in it.

import { boot, vi, press, screen, regrid, put, get, quitvi, is, ok } from "./exlib.mjs";

await boot("viresize");

const LINES = Array.from({ length: 12 }, (_, i) => `line${i + 1}`);
put("/tmp/f", LINES.join("\n") + "\n");

// Twenty-four rows: twelve lines, eleven tildes, and the name.
vi("/tmp/f");
is("at 80x24", screen(14), LINES.join("\n") + "\n~\n~");

// Ten rows: the same twelve lines will not fit, so the window shows what does.
regrid(40, 10);
is("at 40x10", screen(9), LINES.slice(0, 9).join("\n"));

// The cursor is still on line one, and the buffer is untouched: x proves both.
press("x");
is("the cursor survived", screen(2), "ine1\nline2");

// Back to a tall screen, and the whole file is on it again.
regrid(80, 24);
is("at 80x24 again", screen(12),
   ["ine1", ...LINES.slice(1)].join("\n"));

// And what it writes is what it was editing.
press(":");
press("w");
press("CR");
is("what it wrote", get("/tmp/f") ?? "(nothing)",
   ["ine1", ...LINES.slice(1)].join("\n") + "\n");
quitvi();

// The other half of the same thing: the screen a session *starts* on. The
// geometry arrives with the claim, so it has to be read there -- twenty-four
// rows is only the default the option table carries.
const MANY = Array.from({ length: 80 }, (_, i) => `l${i + 1}`);
put("/tmp/g", MANY.join("\n") + "\n");
regrid(144, 41);
vi("/tmp/g");
is("forty lines on a 41-row screen", screen(40), MANY.slice(0, 40).join("\n"));

// And the window followed the screen: ^D is half of it, not half of 24.
press("^D");
is("^D scrolled twenty", screen(1), "l21");
quitvi();

// A resize while the : line is up must not touch the buffer. readecho()
// empties linebuf to compose the command in genbuf, and vresize()'s vsave()
// would take that empty line for an edit and write it over the real one --
// silently, and :w then put the loss on disk.
put("/tmp/f", "alpha\nbeta\ngamma\n");
regrid(80, 24);
vi("/tmp/f");
press(":");
for (const c of "w /tmp/o1")
    press(c);
regrid(60, 20);
press("CR");
is("a resize under : leaves the buffer alone", get("/tmp/o1") ?? "(nothing)",
   "alpha\nbeta\ngamma\n");
quitvi();

// The same for a search, which uses the echo line the same way.
put("/tmp/f", "alpha\nbeta\ngamma\n");
regrid(80, 24);
vi("/tmp/f");
press("/");
for (const c of "beta")
    press(c);
regrid(60, 20);
press("CR");
press(":");
for (const c of "w /tmp/o2")
    press(c);
press("CR");
is("a resize under / leaves the buffer alone", get("/tmp/o2") ?? "(nothing)",
   "alpha\nbeta\ngamma\n");
quitvi();

// But an edit in flight still has to be saved: the guard is splitw, not a
// blanket skip.
put("/tmp/f", "alpha\nbeta\ngamma\n");
regrid(80, 24);
vi("/tmp/f", ["i", "Z"]);
regrid(60, 20);
press("ESC");
press(":");
for (const c of "w /tmp/o3")
    press(c);
press("CR");
is("a resize mid-insert keeps the insertion", get("/tmp/o3") ?? "(nothing)",
   "Zalpha\nbeta\ngamma\n");
quitvi();

ok("a resize re-cuts the screen, the geometry is read at the claim, the window follows, and the buffer survives one under :");
