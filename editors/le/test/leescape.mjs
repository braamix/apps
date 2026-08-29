// ESC, on the first press.
//
// It took two while `escape` was \e|\e and 55 other bindings were \e|X: the
// ESC node carried no action and GetNextAction blocked there. ESC has a code
// of its own now, and the meta bindings are reached with Alt.

import { boot, le, put, press, keys, screen, status, tick, is, ok, H } from "./lelib.mjs";

await boot("leescape");
put("/tmp/f", "alpha\nbeta\ngamma\ndelta\n");
le("/tmp/f");

// The menu bar: one press leaves it.
press("^n");
tick(2);
is("the menu bar is up", screen(1),
   "   File  Edit  Block  Search  Move  Format  Others  Options  Help");
press("ESC");
tick(2);
is("and one ESC leaves it", screen(1), "alpha");

// A prompt: one press cancels it, and the buffer is untouched.
press("^f");
tick(2);
is("the search prompt", status(), "Search forwards:");
press("ESC");
tick(2);
is("and one ESC cancels it", status().slice(0, 16), "Line=1     Col=1");
is("the buffer is as it was", screen(4), "alpha\nbeta\ngamma\ndelta");

// The meta bindings, which the ESC prefix used to serve. \e|a is
// beginning-of-line and \e|z is redo.
keys("END", "M-a");
tick(2);
is("Alt-a goes to the beginning of the line", `${H.screen().cursor_x},${H.screen().cursor_y}`, "0,0");
keys("X", "^u");
tick(2);
is("undo drops a typed character", screen(1), "alpha");
press("M-z");
tick(2);
is("and Alt-z redoes it", screen(1), "Xalpha");

// And in the editor: one ESC asks about the modified buffer.
press("ESC");
tick(3);
is("one ESC raises the quit prompt", screen(15).split("\n").slice(8).join("\n"),
`                     ┌───────────────────────────────────┐
                     │                                   │
                     │ The file has been modified. Save? │
                     │                                   │
                     │      Yes        No      Cancel    │
                     │                                   │
                     └───────────────────────────────────┘`);

ok("one ESC acts, and Alt still reaches the meta bindings");
