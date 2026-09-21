// -V, -h, a bad colour, q, and screensaver mode.

import { boot, die, get, H, ok, press, start, submit, tick } from "./cmlib.mjs";

await boot("keys");

// 1. -V and -h write upstream's text to stdout and leave without a screen.
submit("cmatrix -V >/tmp/v");
const ver = get("/tmp/v") ?? "";
if (!ver.includes("CMatrix version 1.2a by Chris Allegretta"))
    die(`-V: ${JSON.stringify(ver)}`);
if (!ver.includes("cmatrix@asty.org")) die(`-V has no email: ${JSON.stringify(ver)}`);

submit("cmatrix -h >/tmp/h");
const help = get("/tmp/h") ?? "";
if (!help.includes("Usage: cmatrix -[abBfhlsVx]")) die(`-h: ${JSON.stringify(help)}`);
if (!help.includes("Screensaver")) die("-h has no screensaver line");

submit("cmatrix -C purple >/tmp/c");
const bad = get("/tmp/c") ?? "";
if (!bad.includes("Invalid color selection")) die(`bad -C: ${JSON.stringify(bad)}`);
const afterBad = H.rows(H.screen());
if (!afterBad.some((l) => l.startsWith(H.prompt(1)))) die("bad -C did not exit 1");

// 2. q ends it, and the shell's screen comes back with a zero status.
const MARK = "matrix-was-here";
submit(`echo ${MARK}`);
if (!H.rows(H.screen()).some((l) => l === MARK)) die("the mark was not echoed");

start();
tick(8);
if (H.rows(H.screen()).some((l) => l === MARK)) die("the rain did not take the screen");

press("q");
tick(2);
const back = H.rows(H.screen());
if (!back.some((l) => l === MARK)) die("q did not restore the shell screen");
if (!back.some((l) => l.startsWith(H.prompt())))
    die(`no zero-status prompt after q: ${back.filter((l) => l.trim()).join("|")}`);

ok("-V, -h, bad -C, and q");

// 3. -s: the first keystroke exits.
submit(`echo ${MARK}`);
start("-s");
tick(8);
if (H.rows(H.screen()).some((l) => l === MARK)) die("-s did not take the screen");
press("x");
tick(2);
const saved = H.rows(H.screen());
if (!saved.some((l) => l === MARK)) die("-s did not exit on a key");

ok("screensaver exits on a key");
