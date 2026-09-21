// ^C ends it, and the screen the shell had comes back.
//
// The program parks on its sleep, so unlike a compute-bound one there is a
// window between two frames to press it in. SIG_INT is caught and is finish():
// status 0, not 130, which is what the signal handler returned.

import { boot, die, H, ok, start, tick } from "./cmlib.mjs";

await boot("interrupt");

const MARK = "matrix-was-here";
H.submit(`echo ${MARK}`, 1);
if (!H.rows(H.screen()).some((l) => l === MARK)) die("the mark was not echoed");

start();
tick(8);
if (H.rows(H.screen()).some((l) => l === MARK)) die("the rain did not take the screen");
if (!H.rows(H.screen()).some((l) => l.trim() !== "")) die("nothing was painted");

H.press("c".codePointAt(0), H.CTRL);
tick(2);

const back = H.rows(H.screen());
if (!back.some((l) => l === MARK)) die("the shell's screen did not come back");
const prompt = back[back.length - 1] ?? "";
if (back.some((l) => l.startsWith(H.prompt(130))))
    die(`^C left 130, expected finish()'s 0: ${prompt}`);

ok("^C, 0, and the screen restored");
