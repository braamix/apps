// Suspends a game to a file and resumes from it, headless.
//
// make test never reached save() or restore() before, and the save format is
// this port's own. Two things are asserted: the same prefix suspended twice
// writes the same bytes, and a resume parses the file it wrote.
//
// The resume does not get to play on. The harness clock is frozen, so no time
// passes between the suspend and the resume, and start()'s fifteen-minute gate
// refuses anything under a third of that -- which is upstream's rule, not this
// port's. Reaching that refusal already means restore() read the file and that
// saved_ and savet_ came back intact.

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
        console.error("usage: suspend.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>] [--seed=<n>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`suspend: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

const H = await import(join(CORE, "test/system/harness.mjs"));

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/adventure", new Uint8Array(readFileSync(opt.binary)));

const enc = new TextEncoder();
const dec = new TextDecoder();

// Far enough in to have moved, taken things and set flags, and short enough to
// stay a fast test. suspend answers the filename prompt with the next line.
const PREFIX = [
    "no", "in", "take lamp", "xyzzy", "on", "w", "w", "w",
    "take cage", "w", "take rod", "w", "w", "drop rod", "take bird",
    "take rod", "w", "w", "d", "suspend", "y",
];

// The name typed at the prompt is a path, so it has to be one the store can be
// read back from.
const suspend_to = (n) => {
    const script = `/tmp/w${n}`, out = `/tmp/o${n}`, save = `/tmp/s${n}`;
    H.store.files.set(script, enc.encode(PREFIX.join("\n") + "\n" + save + "\n"));
    const cmd = `ADVENTURE_SEED=${opt.seed} adventure <${script} >${out}`;
    if (cmd.length > 60)
        die(`the command line is ${cmd.length} keys, and the ring holds 64`);
    H.submit(cmd, 100);
    if (H.run(101) !== -1)
        die("the game left the kernel with work to do");
    return [dec.decode(H.store.files.get(out) || new Uint8Array(0)),
            H.store.files.get(save)];
};

// 1. It suspends, and says so.
const [log1, save1] = suspend_to(1);
if (!/That should do it\.  Gis revido\./.test(log1))
    die(`the game did not suspend; it said ${JSON.stringify(log1.slice(-200))}`);
if (!save1 || !save1.length)
    die("suspend wrote no file");
const said = /Saved (\d+) bytes to/.exec(log1);
if (!said)
    die("suspend never reported a size");
if (Number(said[1]) !== save1.length)
    die(`it reported ${said[1]} bytes and wrote ${save1.length}`);

// 2. The same prefix suspends to the same bytes. The state is one field list,
//    so a field the writer forgot would show up as a length that moved.
const [log2, save2] = suspend_to(2);
if (save1.length !== save2.length)
    die(`two runs of the same prefix wrote ${save1.length} and ${save2.length} bytes`);
for (let i = 0; i < save1.length; i++)
    if (save1[i] !== save2[i])
        die(`two runs of the same prefix differ at byte ${i}`);
const nopath = (s) => s.replace(/\/tmp\/s\d/g, "<save>");
if (nopath(log1) !== nopath(log2))
    die("two runs of the same prefix said different things");

// 3. It resumes: restore() parses the file, and start() then refuses to carry
//    on because no time has passed. "forged file" is what a file restore()
//    rejected prints, so its absence is the assertion that matters.
H.store.files.set("/tmp/e", enc.encode("\n"));
H.submit("adventure /tmp/s1 </tmp/e >/tmp/r", 100);
if (H.run(101) !== -1)
    die("the resume left the kernel with work to do");
const resumed = dec.decode(H.store.files.get("/tmp/r") || new Uint8Array(0));
if (/forged file/.test(resumed))
    die(`restore() rejected the file it just wrote: ${JSON.stringify(resumed.slice(0, 200))}`);
if (!/This adventure was suspended a mere 0 minutes ago\./.test(resumed))
    die(`the resume did not reach the latency gate; it said ` +
        `${JSON.stringify(resumed.slice(0, 300))}`);

// 4. A file that is not a save file is refused rather than mis-parsed.
H.store.files.set("/tmp/junk", new Uint8Array(save1.length).fill(0x41));
H.submit("adventure /tmp/junk </tmp/e >/tmp/j", 100);
if (H.run(101) !== -1)
    die("the forgery left the kernel with work to do");
const forged = dec.decode(H.store.files.get("/tmp/j") || new Uint8Array(0));
if (!/forged file/.test(forged))
    die(`a file of 'A's was accepted as a save: ${JSON.stringify(forged.slice(0, 200))}`);

console.log(`suspend ok: ${save1.length} bytes, reproducible, and it reads back`);
