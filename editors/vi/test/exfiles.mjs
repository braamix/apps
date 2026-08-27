// Reading and writing: :w in its several forms, :r, :e, :n and the arg list,
// and the two refusals that stop a buffer being lost.

import { boot, ex, put, get, is, ok } from "./exlib.mjs";

await boot("exfiles");

const FILE = "alpha\nbeta\ngamma\n";
const HEAD = `"/tmp/f" 3 lines, 17 characters`;

// A plain write, and a write of part of the buffer.
put("/tmp/f", FILE);
ex("/tmp/f", "w /tmp/w\n1,2w /tmp/p\nq\n");
is("whole buffer", get("/tmp/w") ?? "(nothing)", FILE);
is("a range", get("/tmp/p") ?? "(nothing)", "alpha\nbeta\n");

// Appending with w>>.
put("/tmp/f", FILE);
put("/tmp/w", "first\n");
ex("/tmp/f", "1w>> /tmp/w\nq\n");
is("w>> appends", get("/tmp/w") ?? "(nothing)", "first\nalpha\n");

// :r puts a file after the addressed line.
put("/tmp/f", FILE);
put("/tmp/r", "one\ntwo\n");
is("read in", ex("/tmp/f", "1r /tmp/r\n%p\nq!\n"),
`${HEAD}
"/tmp/r" 2 lines, 8 characters
alpha
one
two
beta
gamma`);

// A buffer with changes in it will not be abandoned without a !.
put("/tmp/f", FILE);
is("quit refuses", ex("/tmp/f", "1d\nq\n%p\nq!\n"),
`${HEAD}
beta
No write since last change (:quit! overrides)
beta
gamma`);

// :w with no name writes the file that was read.
put("/tmp/f", FILE);
ex("/tmp/f", "2d\nw\nq\n");
is("bare :w", get("/tmp/f") ?? "(nothing)", "alpha\ngamma\n");

// readonly refuses a write of the file it was set for, and :w! overrides.
put("/tmp/f", FILE);
is("readonly", ex("/tmp/f", "set ro\n1d\nw\nq!\n"),
`${HEAD}
beta
"/tmp/f" File is read only`);

// The arg list: :n moves to the next file, :rew goes back to the first.
put("/tmp/f", FILE);
put("/tmp/g", "second\n");
is("next and rewind", ex("/tmp/f /tmp/g", "n\n%p\nrew\n%p\nq\n", ""),
`2 files to edit
${HEAD}
"/tmp/g" 1 line, 7 characters
second
2 files to edit
${HEAD}
alpha
beta
gamma
1 more file to edit`);

ok(":w, :w>>, a range, :r, :e refusals, readonly, :n and :rew");
