// The data files the Options menus load, through the installed package.
//
// share/colors-* and share/keymap-emacs are payload like the syntax rules: the
// menu names them and epath resolves them under the store directory, so a file
// missing from the package is a menu entry that reports an error. The two
// "Save as terminal specific" entries that used to sit here are gone with TERM
// -- one terminal, no terminfo, so they wrote a second copy of what the Save
// entry above them already wrote.

import { boot, le, put, press, screen, paint, cursor, tick, is, ok } from "./lelib.mjs";

await boot("ledata");
put("/tmp/f", "alpha\nbeta\ngamma\n");
le("/tmp/f");

// Before any menu covers the text.
const before = paint(0, 0);

// By accelerator throughout -- the bar reopens on whatever was last chosen, so
// counting arrows from File only works the first time.
function options(letter) {
    press("^n");
    tick(2);
    press("o");
    tick(2);
    press(letter);
    tick(2);
}

options("c");
is("the Colors menu offers the schemes", String(screen().includes("Load white")), "true");
is("and no longer a terminal-specific save",
   String(screen().includes("terminal specific")), "false");

// Down to "Load white": Edit, Save, Default, then the five Load entries.
for (let i = 0; i < 7; i++)
    press("DOWN");
press("CR");
tick(3);
is("the default palette", before, "a white/blue");
is("and Load white repaints from share/colors-white", paint(0, 0), "a black/white");

options("k");
is("the Keyboard map menu offers the emacs one",
   String(screen().includes("Load Emacs-like keymap")), "true");
is("and no longer a terminal-specific save",
   String(screen().includes("terminal specific")), "false");

press("DOWN");
press("CR");
tick(4);
is("loading it leaves the text alone", screen(3), "alpha\nbeta\ngamma");
is("and the cursor at the top", cursor(), "0,0");

// The motions the map is for, out of share/keymap-emacs.
press("^f");
tick(1);
is("^F is forward-char", cursor(), "1,0");
press("^n");
tick(1);
is("^N is next-line", cursor(), "1,1");
press("^e");
tick(1);
is("^E is end-of-line", cursor(), "4,1");
press("^a");
tick(1);
is("^A is beginning-of-line", cursor(), "0,1");
press("^p");
tick(1);
is("^P is previous-line", cursor(), "0,0");

ok("the colour schemes and the emacs keymap, out of the package");
