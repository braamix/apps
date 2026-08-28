// Visual mode: the commands that change the buffer, and the file they leave.
//
// The screen says the display kept up; the file says the buffer was right.
// Both matter, and they are different assertions.

import { boot, vi, press, tick, screen, put, get, quitvi, is, ok } from "./exlib.mjs";

await boot("viinsert");

const FILE = "alpha\nbeta\ngamma\ndelta\n";

function edit(keys, want, what) {
    put("/tmp/f", FILE);
    vi("/tmp/f", keys);
    is(what, screen(4), want);
}

// Insert, append, and open a line above and below.
edit(["i", "XY", "ESC"], "XYalpha\nbeta\ngamma\ndelta", "i");
edit(["A", "!", "ESC"], "alpha!\nbeta\ngamma\ndelta", "A");
edit(["o", "new", "ESC"], "alpha\nnew\nbeta\ngamma", "o");
edit(["O", "new", "ESC"], "new\nalpha\nbeta\ngamma", "O");

// Delete and change, with an operator and a motion.
edit(["d", "d"], "beta\ngamma\ndelta\n~", "dd");
edit(["2", "d", "d"], "gamma\ndelta\n~\n~", "2dd");
edit(["c", "w", "ZZZ", "ESC"], "ZZZ\nbeta\ngamma\ndelta", "cw");
edit(["d", "w"], "\nbeta\ngamma\ndelta", "dw");
edit(["r", "Z"], "Zlpha\nbeta\ngamma\ndelta", "r");
edit(["J"], "alpha beta\ngamma\ndelta\n~", "J");
edit([">", ">"], "        alpha\nbeta\ngamma\ndelta", ">>");

// Yank and put, and the delete register.
edit(["y", "y", "p"], "alpha\nalpha\nbeta\ngamma", "yy p");
edit(["d", "d", "p"], "beta\nalpha\ngamma\ndelta", "dd p");

// u undoes the last change; . repeats it.
edit(["d", "w", "u"], "alpha\nbeta\ngamma\ndelta", "u");

// Undo after moving off the changed line. vsave() syncs the line's undo copy
// out of vutmp when you leave it, and that sync is a yank -- which was being
// dropped, so undo quietly did nothing here.
edit(["x", "j", "u"], "alpha\nbeta\ngamma\ndelta", "u after moving off the line");
edit(["i", "ZZ", "ESC", "j", "u"], "alpha\nbeta\ngamma\ndelta", "u after an insert and a move");
edit(["d", "d", "j", "u"], "alpha\nbeta\ngamma\ndelta", "u after dd and a move");
edit(["x", "j", "."], "lpha\neta\ngamma\ndelta", ".");

// A named buffer.
edit(['"', "a", "y", "y", "j", '"', "a", "p"],
     "alpha\nbeta\nalpha\ngamma", "a named buffer");

// The : escape reaches the whole of command mode and comes back.
edit([":", "set nu", "CR"],
     "     1  alpha\n     2  beta\n     3  gamma\n     4  delta", ": escape");

// :map, :ab, and the two shell escapes. A filter's child writes to a file, so
// the screen is never handed over for one; :! hands it over and takes it back.
edit([":", "map q dd", "CR", "q"], "beta\ngamma\ndelta\n~", ":map");
edit([":", "ab xx hello", "CR", "i", "xx ", "ESC"],
     "hello alpha\nbeta\ngamma\ndelta", ":ab");
// :! pauses on the console with [Hit return to continue]; vibang.mjs asserts
// the pause itself, this only that the space answering it lands back in vi.
put("/tmp/f", FILE);
vi("/tmp/f", [":", "!echo hi", "CR"]);
tick(4);
press(" ");
is(":! comes back", screen(4), "alpha\nbeta\ngamma\ndelta");
edit(["!", "!", "cat", "CR"], "alpha\nbeta\ngamma\ndelta", "!!cat");

// ZZ writes and leaves. The file is what this is really asserting.
put("/tmp/f", FILE);
vi("/tmp/f", ["x", "Z", "Z"]);
is("what ZZ wrote", get("/tmp/f") ?? "(nothing)", "lpha\nbeta\ngamma\ndelta\n");
quitvi();

// The report line, over `report` lines so that it is printed at all. It names
// the command, and the name is the user's word, not the C++ function's.
put("/tmp/g", Array.from({ length: 20 }, (_, i) => `line${i + 1}`).join("\n") + "\n");
vi("/tmp/g", ["1", "0", "d", "d"]);
is("the delete report", screen().split("\n").pop(), "10 lines deleted");
quitvi();

ok("i/A/o/O, dd/cw/dw/r/J/>>, yy/p, u, ., a register, :map, :ab, !, ZZ, report");
