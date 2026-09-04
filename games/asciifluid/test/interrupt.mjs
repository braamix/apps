// ^C ends an endless run, and the screen the shell had comes back.
//
// A typed ^C works here where dhrystone's needs `kill -INT %1`: this program
// parks on a timer every frame, so run() goes idle between frames and there is
// a window to press it in. The claim is what restores the screen — the process
// is killed and runs no destructor of its own.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/games/asciifluid/asciifluid.wasm"),
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: interrupt.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`asciifluid: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

const H = await import(join(CORE, "test/system/harness.mjs"));

// ---------------------------------------------------------------- the run

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/asciifluid", new Uint8Array(readFileSync(opt.binary)));
H.store.files.set("/tmp/c",
                  new Uint8Array(readFileSync(join(APPS, "games/asciifluid/data/logo.txt"))));

// A mark on the shell's screen, which the claim has to put back.
const MARK = "before the fluid";
let now = 1;
H.submit(`echo ${MARK}`, now);
if (!H.rows(H.screen()).some((l) => l === MARK))
    die("the shell did not echo the mark");

now += 1;
H.submit("asciifluid /tmp/c", now);

// ------------------------------------------------------------- assertions

// 1. It is painting, and the mark is gone: the alternate screen is up.
now += 12.5;
H.run(now);
const during = H.rows(H.screen());
if (during.some((l) => l === MARK))
    die("the mark is still on screen, so the program never took it");
if (!during.some((l) => l.includes("#")))
    die(`nothing was painted: ${JSON.stringify(during.filter((l) => l.trim()))}`);

// 2. ^C ends it, and the shell reports 130.
now += 12.5;
H.press("c".codePointAt(0), H.CTRL);
if (H.run(now) !== -1)
    die("the interrupt left the kernel with work to do");
const s = H.screen();
if (H.row(s, s.cursor_y) !== H.prompt(130))
    die(`^C left ${JSON.stringify(H.row(s, s.cursor_y))}, ` +
        `expected ${JSON.stringify(H.prompt(130))}`);

// 3. The screen the shell had is back.
if (!H.rows(s).some((l) => l === MARK))
    die(`the shell's screen did not come back: ` +
        `${JSON.stringify(H.rows(s).filter((l) => l.trim()))}`);

console.log("asciifluid: interrupted, 130, screen restored");
