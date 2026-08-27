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

ok("a resize re-cuts the screen, keeps the buffer and keeps the cursor");
