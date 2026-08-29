// Colour: the frame a window draws around itself.
//
// The box-drawing characters go out through add_wch, which is the one writer
// that takes a cchar_t. Every WACS_* glyph carries A_NORMAL, so a shim that
// does not merge the current attribute paints the frame in pair 0 -- white on
// black -- around a correctly coloured interior.

import { boot, le, put, press, keys, screen, tick, paint, is, ok } from "./lelib.mjs";

await boot("lecolor");
put("/tmp/f", "alpha\nbeta\ngamma\ndelta\n");
le("/tmp/f");
is("the text window", paint(0, 0), "a white/blue");

// The File submenu: MENU_WIN, black on cyan.
press("^n");
tick(2);
press("CR");
tick(2);
is("the submenu is up", screen(2),
`   File  Edit  Block  Search  Move  Format  Others  Options  Help
be┌──────────────────┐`);

is("the top left corner", paint(1, 2), "┌ black/cyan");
is("the top edge", paint(1, 6), "─ black/cyan");
is("the left edge", paint(5, 2), "│ black/cyan");
is("the divider's tee", paint(8, 2), "├ black/cyan");
is("the bottom right corner", paint(16, 21), "┘ black/cyan");
is("the interior", paint(5, 6), "i black/cyan");
is("and the text beside it is untouched", paint(5, 30), "  white/blue");

press("ESC");
tick(2);
is("the text comes back", paint(1, 2), "t white/blue");

// The error box: ERROR_WIN, white on red. A different window, so the frame
// takes the colour of the window rather than one the shim happens to hold.
keys("^f");
tick(2);
keys("[", "CR");
tick(4);
is("the error box is up", screen(9).split("\n").slice(8).join("\n"),
   "                            ┌─────── Error ────────┐");
is("its corner", paint(8, 28), "┌ white/red");
is("its edge", paint(14, 40), "─ white/red");
is("and its message", paint(10, 30), "U white/red");

ok("the frames take their window's colour");
