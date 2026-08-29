// A resize while a window is up: the menu bar, a pull-down, the Replace?
// prompt and a half-typed chord.
//
// The frame half is in lescreen.mjs; this is the layout half. A resize is a
// signal that lands wherever the editor happens to be parked, and every one of
// these parks somewhere other than Edit()'s loop -- so each has to lay itself
// out again rather than fall through to its default and drop the prompt.

import { boot, le, put, keys, press, screen, row, status, bg, tick, is, ok, H } from "./lelib.mjs";

await boot("leresize");
put("/tmp/f", "alpha\nbeta\ngamma\ndelta\n");
le("/tmp/f");

/* The menu bar spans the screen, and it is built once at startup -- so it is
   the one window whose asked-for width has to move with COLS. */
press("^n");
tick(2);
is("the menu bar", row(0),
   "   File  Edit  Block  Search  Move  Format  Others  Options  Help");
const bar = bg(0, 79);

H.regrid(100, 30, "resize returned no screen descriptor");
tick(3);
is("the bar's items are still on it", row(0),
   "   File  Edit  Block  Search  Move  Format  Others  Options  Help");
/* CreateWin froze the bar's width at the COLS it was built for; without a
   refit the last twenty columns keep the text window's colour. */
is("and it spans the new width", bg(0, 99), bar);

/* The pull-down over it: the box is laid out again by WindowsResized and the
   items are painted by the menu's own WINDOW_RESIZE arm. */
for (let i = 0; i < 7; i++)
    press("RIGHT");
press("CR");
tick(2);
H.regrid(64, 18, "resize returned no screen descriptor");
tick(3);
const items = H.rows(H.screen())
    .filter((r) => r.includes("│"))
    .map((r) => r.slice(r.indexOf("│") + 1, r.lastIndexOf("│")).trim().split(/\s{2,}/)[0]);
is("the Options menu survives a resize", items.join(", "),
   "Editor, Format, Undo, Appearance, Colors, Keyboard map, Programs, " +
   "Save to current directory, Update current options file");

press("ESC");
press("ESC");
tick(3);
is("and the text comes back when it closes", screen(4),
`alpha
beta
gamma
delta`);
is("on the new geometry", `${H.screen().cols}x${H.rows(H.screen()).length}`, "64x18");

/* The Replace? prompt is one-shot: it reads a single action and acts on it, so
   a resize reaching its default would drop the prompt rather than ask again. */
keys("^g", "b");
press("^r");
tick(2);
keys("e", "t");
press("ENTER");
keys("X", "Y");
press("ENTER");
tick(2);
is("the replace prompt", status().slice(0, 8), "Replace?");

H.regrid(76, 26, "resize returned no screen descriptor");
tick(3);
is("still up after a resize", status().slice(0, 8), "Replace?");

press("y");
tick(2);
is("and the answer it was waiting for still lands", screen(4),
`alpha
bXYa
gamma
delta`);

/* A resize part-way through a chord: F4 is a prefix as well as an action, so
   the editor is parked one key into the tree. The resize used to wait there
   for a keystroke that might never come. */
press("F4");
H.regrid(80, 24, "resize returned no screen descriptor");
tick(3);
is("a resize mid-chord repaints at once", screen(4),
`alpha
bXYa
gamma
delta`);
is("on the new geometry", `${H.screen().cols}x${H.rows(H.screen()).length}`, "80x24");

ok("a resize under the menu, a dialogue, a prompt and a chord");
