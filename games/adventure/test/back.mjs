// Exercises `back`, which the walkthrough never types.
//
// mback() is the one place upstream compared travel-list pointers for
// identity, so it is the one place the move from a linked list to an indexed
// Vec could go wrong quietly. The script is the walkthrough's opening with a
// `back` after every command, which reaches the three exits that are live:
// the way back found in the list, no remembered way back, and "you can't get
// there from here". The fourth, the forced-location fallback, is unreachable
// as this port has it -- see the note in README.md.
//
// The golden file was generated from the binary as it stood before the travel
// lists changed. Refresh it only when a change to the game is meant:
//   cp build/games/adventure/back.log games/adventure/test/back.log

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";

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
        console.error("usage: back.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>] [--seed=<n>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`back: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

const H = await import(join(CORE, "test/smoke/harness.mjs"));

const LOG = join(APPS, "build/games/adventure/back.log");

const SCRIPT = [
    "no", "back", "east", "back", "get keys", "back", "get lamp", "back",
    "west", "back", "south", "back", "south", "back", "south", "back",
    "unlock grate", "back", "down", "back", "west", "back", "light lamp",
    "back", "get cage", "back", "west", "back", "west", "back", "west",
    "back", "get bird", "back", "west", "back", "down", "back", "south",
    "back", "get gold", "back", "north", "back", "north", "back",
    "drop bird", "back", "drop cage", "back", "drop keys", "back", "south",
    "back", "get jewelry", "back", "north", "back", "sw", "back", "west",
    "back", "kill dragon", "back", "yes", "back", "get rug", "back",
    "quit", "y",
];

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/adventure", new Uint8Array(readFileSync(opt.binary)));
H.store.files.set("/tmp/b", new TextEncoder().encode(SCRIPT.join("\n") + "\n"));

const cmd = `ADVENTURE_SEED=${opt.seed} adventure </tmp/b >/tmp/bo`;
if (cmd.length > 60)
    die(`the command line is ${cmd.length} keys, and the ring holds 64`);
H.submit(cmd, 100);
if (H.run(101) !== -1)
    die("the game left the kernel with work to do");

const log = new TextDecoder().decode(H.store.files.get("/tmp/bo") || new Uint8Array(0));
if (!log)
    die("the game wrote nothing");
mkdirSync(dirname(LOG), { recursive: true });
writeFileSync(LOG, log);

// The live exits showed up. Without these the diff below could pass on a
// transcript that never reached the routine at all.
const EXITS = [
    ["no longer seem to remember how it was you got here", "the unremembered way back"],
    ["You can't get there from here", "the same-place refusal"],
];
for (const [want, why] of EXITS)
    if (!log.includes(want))
        die(`${why} never happened — the script no longer reaches it`);

const GOLDEN = join(HERE, "back.log");
if (!existsSync(GOLDEN))
    die(`no golden transcript at ${GOLDEN}`);
const want = readFileSync(GOLDEN, "utf8").split("\n");
const lines = log.split("\n");
const found = lines.findIndex((l, i) => l !== want[i]);
const at = found < 0 ? Math.min(lines.length, want.length) : found;
if (found >= 0 || lines.length !== want.length) {
    console.error(`back: the transcript is ${LOG}`);
    die(`it differs from ${GOLDEN} at line ${at + 1}: ` +
        `${JSON.stringify(lines[at])} for ${JSON.stringify(want[at])}`);
}

console.log(`back ok: ${SCRIPT.length} commands, ${lines.length} lines`);
