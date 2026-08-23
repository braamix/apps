// Plays walkthrough.txt against adventure.wasm under braam-core's smoke
// harness, headless; node pumps the kernel's clock by hand.
//
// Input is a file, not the keyboard, so the game prints no prompt and echoes
// nothing — the "> " belongs to the LineEditor alone. Output goes to a file
// too: a three-hundred-turn transcript does not fit on 24 rows. The commands
// are put back into it afterwards, one per turn, so it reads like a session.

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
    // The seed the walkthrough was written against and the score it reaches.
    // Move the seed and the dwarves move.
    seed: "36",
    floor: "350",
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary|seed|floor)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: play.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>] [--seed=<n>] [--floor=<score>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}
const FLOOR = Number(opt.floor);

const die = (msg) => {
    console.error(`adventure: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

// After the paths are checked: the harness exits the process itself, and would
// not say what to build.
const H = await import(join(CORE, "test/smoke/harness.mjs"));

const LOG = join(APPS, "build/games/adventure/game.log");

// ---------------------------------------------------------------- the run

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
if (!H.store.files.has("/bin/sh"))
    die("the archive did not unpack");

// Planted, not packed: exec takes any stamped path, and /bin is ordinary store
// content once boot has unpacked the archive.
H.store.files.set("/bin/adventure", new Uint8Array(readFileSync(opt.binary)));

const script = readFileSync(join(HERE, "walkthrough.txt"), "utf8")
    .split("\n").filter((l) => l.length && !l.startsWith("#"));
H.store.files.set("/tmp/w", new TextEncoder().encode(script.join("\n") + "\n"));

// Short paths on purpose: the key ring holds 64 and type() does not check.
const cmd = `ADVENTURE_SEED=${opt.seed} adventure </tmp/w >/tmp/g`;
if (cmd.length > 60)
    die(`the command line is ${cmd.length} keys, and the ring holds 64`);

// The game flushes its buffer once before every read and once on the way out,
// so the writes to the log are its turns — which is what puts the commands back
// into it. `stamp` is the fake store's per-write hook, and the file has already
// grown by then, so its length is the boundary.
const cuts = [];
const stamp = H.store.stamp.bind(H.store);
H.store.stamp = (path) => {
    const n = path === "/tmp/g" && H.store.files.get(path).length;
    if (n > (cuts[cuts.length - 1] || 0))   // the redirect truncates first
        cuts.push(n);
    stamp(path);
};

const started = Date.now();
let s = H.submit(cmd, 100);
const took = Date.now() - started;
H.store.stamp = stamp;

const log = new TextDecoder().decode(H.store.files.get("/tmp/g") || new Uint8Array(0));

// The flush before a read carries the turn before it, so the first chunk is
// what the game said before asking anything and chunk i + 1 answers command i.
const chunks = cuts.map((end, i) =>
    log.slice(i ? cuts[i - 1] : 0, end).replace(/^\n+|\n+$/g, ""));
if (chunks.length !== script.length + 1)
    die(`${chunks.length} turns of output for ${script.length} commands`);
const shown =
    chunks.flatMap((c, i) => (i ? [`> ${script[i - 1]}`, c] : [c])).join("\n\n") + "\n";
mkdirSync(dirname(LOG), { recursive: true });
writeFileSync(LOG, shown);

const fail = (msg) => {
    console.error(`adventure: ${msg}`);
    console.error(`adventure: the transcript is ${LOG}`);
    process.exit(1);
};

// ------------------------------------------------------------- assertions

// 1. It ran to the end and the shell got its prompt back with status 0.
if (H.run(101) !== -1)
    fail("the game left the kernel with work to do");
s = H.screen();
if (H.row(s, s.cursor_y) !== H.prompt())
    fail(`the game exited to ${JSON.stringify(H.row(s, s.cursor_y))}, ` +
         `expected ${JSON.stringify(H.prompt())}`);
if (!log)
    fail("the game wrote nothing");

// Messages wrap, so match against one flat string.
const flat = log.replace(/\s+/g, " ");
// Where in the transcript a match landed, as a percentage and the text around it.
const near = (at) => `${Math.round((100 * at) / flat.length)}% in: ` +
                     JSON.stringify(flat.slice(Math.max(0, at - 90), at + 60));

// 2. Nothing went off the rails. The first three are glorkz's parser refusals:
//    finding one means a line of the walkthrough is not in this vocabulary,
//    which is what catches `all`, `give` and `wait`. "You can't go that way" is
//    not one of them and is normal. The fourth means a question ate a command
//    and everything after it answers the wrong prompt; the last, a dwarf won.
const WRONG = [
    ["I don't know that word.", "a word this vocabulary does not have"],
    ["I don't understand that!", "two words that do not go together"],
    ["I don't know how to apply that word here.", "a word that does not apply here"],
    ["Please answer the question.", "a question a command was fed to"],
    ["you seem to have gotten yourself killed", "a death"],
];
for (const [bad, why] of WRONG) {
    const at = flat.indexOf(bad);
    if (at >= 0)
        fail(`${why}, ${near(at)}`);
}

// 3. The landmarks it passes, in order. A failure names the first one missed,
//    which is where the run diverged.
const LANDMARKS = [
    "You are standing at the end of a road before a small brick building.",
    "You are inside a building, a well house for a large spring.",
    "The grate is now unlocked.",
    "A note on the wall says \"magic word xyzzy\".",
    "The little bird attacks the green snake",         // the bird earns its cage
    "Congratulations! You have just vanquished a dragon with your bare",
    "A hollow voice says \"plugh\".",
    "The plant spurts into furious growth for a few seconds.",
    "You're in a small chamber lit by an eerie green light.",   // the plover room
    "A glistening pearl falls out of the clam and rolls away.",
    "The troll catches your treasure and scurries away out of sight.",
    "The bear eagerly wolfs down your food",
    "The bear lumbers toward the troll",               // the toll paid a second way
    "Out from the shadows behind you pounces a bearded pirate!",
    "The pirate's treasure chest is here!",
    "There are a few recent issues of \"Spelunker today\" magazine here.",
    "A sepulchral voice reverberating through the cave, says, \"Cave closing",
    "The sepulchral voice entones, \"The cave is now closed.\"",
    "It appears to be a repository for the \"adventure\" program.",
    "there is a loud explosion, and a twenty-foot hole appears in the far",
];
let from = 0;
for (const want of LANDMARKS) {
    const at = flat.indexOf(want.replace(/\s+/g, " "), from);
    if (at < 0)
        fail(`the walkthrough never reached ${JSON.stringify(want)}` +
             (from ? `; it got as far as ${near(from)}` : ""));
    from = at + want.length;
}

// 4. The final score. The `score` command words its own differently.
const end = /You scored (\d+) out of a possible (\d+) using (\d+) turns/.exec(log);
if (!end)
    fail("the game never scored itself — it did not reach the end");
const [, got, possible, turns] = end;
if (Number(got) < FLOOR)
    fail(`scored ${got}/${possible}, expected at least ${FLOOR}`);

// 5. The transcript itself, against the copy checked in beside the
//    walkthrough. The run is deterministic, so this is exact; refresh it with
//    `cp build/games/adventure/game.log games/adventure/test/game.log` when a
//    change to the game is meant.
const GOLDEN = join(HERE, "game.log");
if (existsSync(GOLDEN)) {
    const want = readFileSync(GOLDEN, "utf8").split("\n");
    const lines = shown.split("\n");
    const found = lines.findIndex((l, i) => l !== want[i]);
    const at = found < 0 ? Math.min(lines.length, want.length) : found;
    if (found >= 0 || lines.length !== want.length)
        fail(`the transcript differs from ${GOLDEN} at line ${at + 1}: ` +
             `${JSON.stringify(lines[at])} for ${JSON.stringify(want[at])}`);
}

const RANK = new RegExp('You have achieved the rating: "([^"]+)"' +
                        "|Your score puts you in ([^.]+)\\." +
                        "|(All of adventuredom gives tribute to you[^\\n]*)");
const rating = RANK.exec(log);
const rank = rating && (rating[1] || rating[2] || rating[3] || "").trim();
console.log(`adventure ok: ${got}/${possible} in ${turns} turns` +
            (rank ? ` — ${rank}` : "") +
            ` (${script.length} commands, ${shown.split("\n").length} lines, ${took} ms)`);
console.log(`adventure: the transcript is ${LOG}`);
