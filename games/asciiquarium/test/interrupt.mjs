// ^C ends it, and the screen the shell had comes back.
//
// The program parks on its 100 ms sleep, so unlike a compute-bound one there
// is a window between two frames to press it in. Nothing catches SIG_INT here
// — upstream drops Ctrl-C silently and only q quits — so the kernel kills the
// process and the claim is what restores the screen.

import { boot, die, H, ok, start, tick } from "./aqlib.mjs";

await boot("interrupt");

const MARK = "aquarium-was-here";
H.submit(`echo ${MARK}`, 1);
if (!H.rows(H.screen()).some((l) => l === MARK)) die("the mark was not echoed");

start();
tick(2);
if (H.rows(H.screen()).some((l) => l === MARK)) die("the aquarium did not take the screen");
if (!H.rows(H.screen()).some((l) => l.includes("~~~~~~~~"))) die("nothing was painted");

H.press("c".codePointAt(0), H.CTRL);
tick(2);

const back = H.rows(H.screen());
if (!back.some((l) => l === MARK)) die("the shell's screen did not come back");
if (back.some((l) => l.includes("~~~~~~~~"))) die("the aquarium is still on the screen");
if (!back.some((l) => l.startsWith(H.prompt(130)))) die(`no [130] prompt: ${back.join("|")}`);

ok("^C, 130, and the screen restored");
