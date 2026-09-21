import { boot, die, frame, get, H, ok, press, start, submit, tick } from "./acl.mjs";

await boot("keys");

// 1. -h and --help write the usage block to stdout and leave without a screen.
submit("asciiclock -h >/tmp/h");
const help = get("/tmp/h") ?? "";
if (!help.includes("Usage:")) die(`-h: ${JSON.stringify(help)}`);
if (!help.includes("q, x, ESC")) die(`-h has no quit keys: ${JSON.stringify(help)}`);
if (!help.includes("toggle the 3D cube")) die(`-h has no f: ${JSON.stringify(help)}`);

submit("asciiclock --help >/tmp/hh");
const helpLong = get("/tmp/hh") ?? "";
if (helpLong !== help) die(`--help differs from -h: ${JSON.stringify(helpLong)}`);

submit("asciiclock -x >/tmp/bad 2>/tmp/e");
const bad = get("/tmp/e") ?? "";
if (!bad.includes("Usage:")) die(`bad flag: ${JSON.stringify(bad)}`);
const afterBad = H.rows(H.screen());
if (!afterBad.some((l) => l.startsWith(H.prompt(2))))
    die("a bad flag did not exit 2");

ok("-h, --help, and a bad flag");

// 2. f toggles the cube; q exits zero and the shell's screen comes back.
const MARK = "clock-was-here";
submit(`echo ${MARK}`);
if (!H.rows(H.screen()).some((l) => l === MARK)) die("the mark was not echoed");

const before = frame();
start();
tick(8);
if (H.rows(H.screen()).some((l) => l === MARK)) die("the clock did not take the screen");

press("f");
tick(4);
const toggled = frame();
if (toggled === before) die("f did not change the picture");

press("q");
tick(2);
const back = H.rows(H.screen());
if (!back.some((l) => l === MARK)) die("q did not restore the shell screen");
if (!back.some((l) => l.startsWith(H.prompt())))
    die(`no zero-status prompt after q`);

ok("f changes the frame and q exits");
