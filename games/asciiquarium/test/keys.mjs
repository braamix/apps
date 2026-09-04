// The five keys: pause, unpause, info, ESC, reset and quit.
//
// A key takes two frames to land — the first wakes the keyboard task, the
// second is the frame the clock draws after it — which is what press() does.

import { boot, die, frame, H, ok, press, row, start, tick } from "./aqlib.mjs";

await boot("keys");
start();
tick(2);

// 1. p freezes the frame. Nothing is repainted while paused, so the screen
// stands still rather than being redrawn identically.
press("p");
const a = frame();
tick(3);
if (frame() !== a) die("p did not pause");

// 2. p again lets it go.
press("p");
tick();
if (frame() === a) die("p did not unpause");

// 3. i puts the overlay up, centred, and pauses under it.
press("i");
const info = frame().split("\n");
const box = info.findIndex((l) => l.includes("╔"));
if (box < 0) die("i did not show the overlay");
if (!info[box + 2].includes("Asciiquarium 2.2.0")) die(`no title: ${info[box + 2]}`);
if (!info.some((l) => l.includes("Q/q quit"))) die("no controls line");
if (!info.some((l) => l.includes("Press I or ESC"))) die("no hint line");
if (info.some((l) => l.includes("~~~~~~~~"))) die("the aquarium is still under the overlay");

// 4. ESC takes it down and lets the aquarium go again.
press("ESCAPE");
const back = frame();
if (back.includes("╔")) die("ESC did not close the overlay");
tick(2);
if (frame() === back) die("ESC did not unpause");

// 5. r respawns the scene: the water and the castle are back where they were.
press("r");
tick();
if ((row(5).match(/~/g) ?? []).length < 60) die(`no water after r: ${row(5)}`);
if (!row(10).includes("T~~")) die(`no castle after r: ${row(10)}`);

// 6. q ends it, and the shell's screen comes back with a zero status.
press("q");
tick(2);
const shell = H.rows(H.screen());
if (!shell.some((l) => l.includes("asciiquarium"))) die("the shell screen did not come back");
if (shell.some((l) => l.includes("~~~~~~~~"))) die("the aquarium is still on the screen");

ok("pause, info, ESC, reset and quit");
