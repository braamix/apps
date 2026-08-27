// The shell escapes: :!, :r !, :w ! and a range filtered through a command.
//
// Upstream forked. There is no fork here, so :! is spawn() with the console
// handed over and taken back, and a filter -- which upstream drove through two
// pipes, writing one of them from a *second copy of the editor* -- is two temp
// files instead. One task cannot park on both ends of a pipeline.

import { boot, ex, put, get, is, ok } from "./exlib.mjs";

await boot("exbang");

const FILE = "alpha\nbeta\ngamma\n";
const HEAD = `"/tmp/f" 3 lines, 17 characters`;

// :r ! reads a command's output in after the addressed line.
put("/tmp/f", FILE);
is("read from a command", ex("/tmp/f", "1r !echo hello\n%p\nq!\n"),
`${HEAD}
!
hello
alpha
hello
beta
gamma`);

// A range through a filter and back. cat is the identity, which is the case
// worth having: it proves the range went out and came back in one piece.
put("/tmp/f", FILE);
is("filtered through cat", ex("/tmp/f", "1,$!cat\n%p\nq!\n"),
`${HEAD}
!
gamma
alpha
beta
gamma`);

// :w ! sends the buffer to a command's standard input.
put("/tmp/f", FILE);
ex("/tmp/f", "1,2w !cat >/tmp/o2\nq!\n");
is("written to a command", get("/tmp/o2") ?? "(nothing)", "alpha\nbeta\n");

// :! on its own runs a command and comes back to the editor. The buffer must
// be exactly as it was: nothing about spawning a child may disturb it.
put("/tmp/f", FILE);
is("plain escape", ex("/tmp/f", "!echo ran\n%p\nq!\n"),
`${HEAD}
ran
!
alpha
beta
gamma`);

ok(":r !, :w !, a filtered range, and :! leaving the buffer alone");
