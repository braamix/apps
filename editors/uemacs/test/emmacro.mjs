// The macro language, which is also the gate on the packaged emacs.rc: it
// ships in the zip beside the binary, under a store path carrying a version
// the binary does not know, so epath.cpp reads the link PATH found to recover
// it and lookup_file() probes there last.

import { boot, em, put, get, press, keys, submit, screen, modeline, is, ok, tick, STORE, H }
    from "./emlib.mjs";

await boot("emmacro");

// emacs.rc ran: it is what turns on spell and utf-8 for a buffer as it is
// read, from the macro bound to META-SPEC-R.
put("/tmp/f", "alpha\nbeta\n");
em("/tmp/f");
is("the packaged emacs.rc ran",
   /\(Spell utf-8\)/.test(modeline()) ? "Spell utf-8" : modeline(), "Spell utf-8");

// A user's own .emacsrc on disk wins over the packaged one. Nothing else
// binds C-x q, so a session that answers it read the file, and the mode line
// says the packaged one did not run.
put("/home/.emacsrc", "bind-to-key beginning-of-file ^Xq\n");
em("/tmp/f", ["^n", "^x", "q"]);
is("$HOME/.emacsrc overrides it, and its binding works", H.screen().cursor_y + "", "0");
is("so the packaged one did not run", /Spell/.test(modeline()) ? "spell" : "no spell",
   "no spell");
H.store.files.delete("/home/.emacsrc");

// So does the system-wide copy, and so does one in the current directory:
// $HOME, /etc, the cwd, $PATH, then the package. The packaged copy is last,
// where upstream's install directories were, so every copy on disk wins over
// it and none of them is shadowed.
//
// Each binds C-x q to a different line, so the cursor says which was read.
put("/etc/emacs.rc", "bind-to-key end-of-file ^Xq\n");
put("/tmp/.emacsrc", "bind-to-key next-line ^Xq\n");
submit("cd /tmp; em f");
press("^x");
press("q");
is("/etc/emacs.rc comes before the current directory",
   H.screen().cursor_y + "", "2");

H.store.files.delete("/etc/emacs.rc");
submit("cd /tmp; em f");
press("^x");
press("q");
is("and the current directory before the packaged copy", H.screen().cursor_y + "", "1");
H.store.files.delete("/tmp/.emacsrc");

// /etc/emacs.hlp is the same for the help file, under its own name.
put("/etc/emacs.hlp", "SYSTEM HELP\n");
em("/tmp/f");
keys("ESC", "?");
tick(3);
is("/etc/emacs.hlp overrides the packaged help",
   H.rows(H.screen())[0].replace(/\s+$/, ""), "SYSTEM HELP");
H.store.files.delete("/etc/emacs.hlp");

// execute-file, and the directives: !if, !else, !endif, !while, !goto and a
// user variable.
put("/tmp/m",
    'set %n 0\n' +
    '!while &less %n 3\n' +
    '\tset %n &add %n 1\n' +
    '\tinsert-string &cat "line " %n\n' +
    '\tnewline\n' +
    '!endwhile\n' +
    '!if &seq %n "3"\n' +
    '\tinsert-string "done"\n' +
    '!else\n' +
    '\tinsert-string "wrong"\n' +
    '!endif\n');
// Written out rather than read off the screen: the buffer starts empty, and
// an empty buffer has no line for the window to be looking at, so what it
// shows after the first insertion is upstream's business and not this test's.
em("");
keys("ESC", "x");
H.type("execute-file");
keys("CR");
H.type("/tmp/m");
keys("CR");
tick(1);
keys("^x", "^w");
H.type("/tmp/out");
keys("CR");
tick(2);
is("!while, !if and the arithmetic", get("/tmp/out"), "line 1\nline 2\nline 3\ndone\n");

// A stored macro, run by number, and a key bound to it.
put("/tmp/m2",
    '3 store-macro\n' +
    '\tbeginning-of-line\n' +
    '\tinsert-string ">> "\n' +
    '!endm\n' +
    'bind-to-key execute-macro-3 ^X#\n');
em("/tmp/f");
keys("ESC", "x");
H.type("execute-file");
keys("CR");
H.type("/tmp/m2");
keys("CR");
tick(1);
keys("^n", "^x", "#");
tick(1);
is("store-macro and bind-to-key", screen(2), "alpha\n>> beta");

// A keyboard macro: C-x ( records, C-x ) ends, C-x e plays it back.
em("/tmp/f");
keys("^x", "(");
H.type("!");
keys("^n", "^a", "^x", ")");
tick(1);
keys("^x", "e");
tick(1);
is("a keyboard macro", screen(2), "!alpha\n!beta");

// $-variables: the ones the editor answers about itself. Through a macro,
// because write-message typed at the prompt takes the text as it stands.
put("/tmp/m3", "write-message &cat \"buffer=\" $cbufname\n");
em("/tmp/f");
keys("ESC", "x");
H.type("execute-file");
keys("CR");
H.type("/tmp/m3");
keys("CR");
tick(1);
is("an environment variable", screen().split("\n").pop(), "buffer=f");

// M-? is the help, which emacs.rc binds to a macro that opens emacs.hlp in a
// window of its own -- so this says both packaged files are reachable.
em("/tmp/f");
keys("ESC", "?");
tick(3);
is("M-? shows the packaged help", H.rows(H.screen())[0].replace(/\s+$/, ""),
   "=>                      uEmacs/PK 4.0 HELP INDEX");
is("and the macro says how to use it", screen().split("\n").pop(),
   "Select topic from list and press <Help>");

// Last, because it takes the package away: with nothing to find, em still
// starts -- no startup file is not an error -- and the help says so.
H.store.files.delete(`${STORE}/share/emacs.rc`);
H.store.files.delete(`${STORE}/share/emacs.hlp`);
em("/tmp/f");
is("without the package emacs.rc does not run",
   /Spell/.test(modeline()) ? "spell" : "no spell", "no spell");
keys("ESC", "?");
tick(3);
is("and the help says it is not there", screen().split("\n").pop(),
   "(Help file is not online)");

ok("emacs.rc, emacs.hlp, the directives, stored macros and a keyboard macro");
