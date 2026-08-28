// Finding files, and the completion that went from a forked shell to list_dir.

import { boot, em, put, get, press, keys, screen, modeline, is, ok, tick, H } from "./emlib.mjs";

await boot("emfiles");

put("/tmp/one", "first\n");
put("/tmp/two", "second\n");
put("/tmp/twelve", "third\n");

// C-x C-f on a file that is there.
em("");
keys("^x", "^f");
H.type("/tmp/one");
keys("CR");
tick(1);
is("C-x C-f", screen(1), "first");
is("the mode line names it", modeline().slice(0, 40),
   "-- uEmacs/Pk 4.0.15: one (Spell utf-8) /");

// And on a file that is not: a new buffer, and the message says so.
keys("^x", "^f");
H.type("/tmp/absent");
keys("CR");
tick(1);
is("a new file", screen().split("\n").pop(), "(New file)");

// C-x b goes back to a buffer by name.
keys("^x", "b");
H.type("one");
keys("CR");
tick(1);
is("C-x b", screen(1), "first");

// TAB completes a file name against the directory, which upstream did by
// forking a shell to echo a glob into a temporary file. A second TAB offers
// the next match, and when they run out it beeps and starts over.
keys("^x", "^f");
H.type("/tmp/tw");
keys("TAB");
tick(1);
const first = screen().split("\n").pop();
keys("TAB");
tick(1);
const second = screen().split("\n").pop();
is("the two matches, in the order the directory lists them",
   [first, second].join(" | ").replace(/Find file: /g, ""),
   "/tmp/twelve | /tmp/two");
keys("CR");
tick(1);
is("and the one it took", screen(1), "second");

// C-x C-v is view mode: the same file, and the mode line says VIEW.
em("");
keys("^x", "^v");
H.type("/tmp/one");
keys("CR");
tick(1);
is("C-x C-v is view mode", screen(1), "first");
is("and says so", /View/.test(modeline()) ? "View" : modeline(), "View");

// C-x C-r replaces the buffer's contents with another file, keeping the name.
keys("^x", "^r");
H.type("/tmp/two");
keys("CR");
tick(1);
is("C-x C-r", screen(1), "second");

// C-x C-i inserts a file into the buffer rather than replacing it.
em("/tmp/one");
keys("ESC", ">", "^x", "^i");
H.type("/tmp/two");
keys("CR");
tick(1);
is("C-x C-i", screen(2), "first\nsecond");

ok("find, read, insert, and the completion over list_dir");
