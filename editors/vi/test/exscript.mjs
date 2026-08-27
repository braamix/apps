// The command set, driven from a script: addresses, the text commands, the
// line commands, marks and undo, and a write read back out of the store.

import { boot, ex, put, get, is, ok } from "./exlib.mjs";

await boot("exscript");

const FILE = "alpha\nbeta\ngamma\ndelta\n";

// Addresses. The whole grammar in one script: a number, ., $, an offset, a
// range, a search, and % for 1,$.
put("/tmp/f", FILE);
is("addresses", ex("/tmp/f", "2p\n.p\n$p\n1+2p\n2,3p\n/gam/p\n%p\nq\n"),
`"/tmp/f" 4 lines, 23 characters
beta
beta
delta
gamma
beta
gamma
gamma
alpha
beta
gamma
delta`);

// Append, insert and change, each reading its text until a lone dot.
put("/tmp/f", FILE);
is("a i c", ex("/tmp/f", "1a\nA1\n.\n1i\nI1\n.\n$c\nC1\n.\n%p\nq!\n"),
`"/tmp/f" 4 lines, 23 characters
I1
alpha
A1
beta
gamma
C1`);

// Delete, move, copy, join. Each of the four is followed by autoprint's copy
// of the line dot ended up on, which is half of what is being asserted.
put("/tmp/f", FILE);
is("d m co j", ex("/tmp/f", "2d\n$m0\n1co$\n1,2j\n%p\nq!\n"),
`"/tmp/f" 4 lines, 23 characters
gamma
delta
delta
delta alpha
delta alpha
gamma
delta`);

// Delete by its long names, and the two trailing-letter forms ed left behind.
// tailprim() matches on the name string, so this is what catches a rename of
// exdelete() leaking into it: :d alone would still pass.
put("/tmp/f", FILE);
is("delete spelled out", ex("/tmp/f", "2delete\n1dele\n1del\nq!\n"),
`"/tmp/f" 4 lines, 23 characters
gamma
gamma
delta`);

put("/tmp/f", FILE);
is("dp and dl", ex("/tmp/f", "2dp\n1dl\nq!\n"),
`"/tmp/f" 4 lines, 23 characters
gamma
gamma$`);

// And the name the error message uses.
put("/tmp/f", FILE);
is("the name in an error", ex("/tmp/f", "1d ,\nq!\n"),
`"/tmp/f" 4 lines, 23 characters
Extra characters at end of "delete" command`);

// A mark follows its line as other lines move around it: 'a is set on line
// two, line one goes, and 'a is line one now and still the same text.
put("/tmp/f", FILE);
is("marks", ex("/tmp/f", "2ka\n1d\n'ap\n'a,$p\nq!\n"),
`"/tmp/f" 4 lines, 23 characters
beta
beta
beta
gamma
delta`);

// Undo puts back a whole command, however many lines it touched.
put("/tmp/f", FILE);
is("undo", ex("/tmp/f", "1,3d\n%p\nu\n%p\nq!\n"),
`"/tmp/f" 4 lines, 23 characters
delta
delta
alpha
alpha
beta
gamma
delta`);

// A write, read back out of the store rather than out of the transcript.
put("/tmp/f", FILE);
ex("/tmp/f", "2,3d\nw /tmp/w\nq\n");
is("what :w wrote", get("/tmp/w") ?? "(nothing)", "alpha\ndelta\n");

ok("addresses, a/i/c, d/m/co/j, the delete spellings, marks, undo and :w");
