// ^C reaches the binding rather than killing the editor.
//
// A foreground program is sent SIG_INT, not the keystroke; getch.cpp asks for
// the signal with sig_catch and hands the ^C back when sig_take says that is
// what arrived, so the binding LE carries on it -- continue-search -- runs.

import { boot, le, put, keys, press, screen, cursor, tick, is, ok } from "./lelib.mjs";

await boot("lesigint");
put("/tmp/g", "one\ntwo\none\ntwo\n");
le("/tmp/g");

press("^f");
tick(1);
keys("t", "w", "o");
press("ENTER");
tick(2);
is("the first hit", cursor(), "0,1");

press("^c");
tick(2);
is("^C continues the search", cursor(), "0,3");
is("and the editor is still up", screen(1), "one");

ok("^C is a keystroke, not a death");
