// A ^C at the game's prompt: the read is abandoned, the command loop is handed
// a synthetic `quit`, and a second ^C answers the question it asks. On the grid
// rather than through a pipe, which has no keyboard to press.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/games/adventure/adventure.wasm"),
    seed: "36",
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary|seed)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: interrupt.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>] [--seed=<n>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`interrupt: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

const H = await import(join(CORE, "test/smoke/harness.mjs"));

// ---------------------------------------------------------------- the run

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/adventure", new Uint8Array(readFileSync(opt.binary)));

const shown = (s) => JSON.stringify(H.rows(s).filter((l) => l.trim()));
const flat = (s) => H.rows(s).join(" ").replace(/\s+/g, " ");

let s = H.submit(`ADVENTURE_SEED=${opt.seed} adventure`, 100);
if (!flat(s).includes("Welcome to adventure!!"))
    die(`the game did not start: ${shown(s)}`);

s = H.submit("no", 101);
if (!flat(s).includes("You are standing at the end of a road"))
    die(`the game did not begin at the road: ${shown(s)}`);

// ------------------------------------------------------------- assertions

// 1. The first ^C is the DEL: the loop pretends the player typed `quit`, and
//    the quit verb asks before doing it.
H.press("c".codePointAt(0), H.CTRL);
H.run(102);
s = H.screen();
if (!flat(s).includes("^C"))
    die(`the interrupt was not echoed: ${shown(s)}`);
if (!flat(s).includes("Do you really want to quit now?"))
    die(`a ^C did not reach the command loop: ${shown(s)}`);

// 2. The second answers it: an abandoned read is a "yes" to a question. The
//    game scores itself and hands the keyboard back.
H.press("c".codePointAt(0), H.CTRL);
if (H.run(103) !== -1)
    die("the game left the kernel with work to do");
s = H.screen();
if (!/You scored \d+ out of a possible \d+/.test(flat(s)))
    die(`the game did not score itself: ${shown(s)}`);
if (H.row(s, s.cursor_y) !== H.prompt())
    die(`the game exited to ${JSON.stringify(H.row(s, s.cursor_y))}, ` +
        `expected ${JSON.stringify(H.prompt())}`);

// 3. The shell reads again, which is what proves the hand-back: adv_input_done
//    runs on an interrupt, where a cancellation skipped it.
s = H.submit("echo alive", 104);
if (!H.rows(s).includes("alive"))
    die(`the shell did not get the keyboard back: ${shown(s)}`);

const [, score] = /You scored (\d+) out of a possible (\d+)/.exec(flat(s)) || [];
console.log(`interrupt ok: ^C quit and scored ${score || "?"}`);
