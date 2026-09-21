import { boot, die, H, ok, start, submit, tick } from "./acl.mjs";

await boot("interrupt");

const MARK = "clock-was-here";
submit(`echo ${MARK}`);
if (!H.rows(H.screen()).some((l) => l === MARK)) die("the mark was not echoed");

start();
tick(8);
if (H.rows(H.screen()).some((l) => l === MARK)) die("the clock did not take the screen");

H.press("c".codePointAt(0), H.CTRL);
tick(2);

const back = H.rows(H.screen());
if (!back.some((l) => l === MARK)) die("the shell's screen did not come back");
if (back.some((l) => l.startsWith(H.prompt(130))))
    die("^C left 130, expected finish()'s 0");

ok("^C, 0, and the screen restored");
