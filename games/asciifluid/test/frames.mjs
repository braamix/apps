// The first twelve frames of column.txt against frames.log, which came out of
// upstream's own endoh1 binary. The simulation is deterministic — no clock, no
// randomness — so the assertion is the whole screen, byte for byte.
//
// The frames advance because run(now) drives the kernel's timer queue; it is
// proc_now() that is frozen here, and nothing in this program reads it.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/games/asciifluid/asciifluid.wasm"),
    frames: "12",
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary|frames)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: frames.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>] [--frames=<n>]");
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

const LOG = join(APPS, "build/games/asciifluid/frames.log");

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
H.store.files.set("/bin/asciifluid", new Uint8Array(readFileSync(opt.binary)));
H.store.files.set("/tmp/c",
                  new Uint8Array(readFileSync(join(APPS, "games/asciifluid/data/column.txt"))));

const cmd = "asciifluid /tmp/c";
H.type(cmd);
H.press(H.KEY.ENTER);

// One frame a tick: paint, park, and the next tick is past the wait. The
// opening frame stands for a second.
const want = Number(opt.frames);
const shot = [];
let now = 1;
for (let k = 0; k < want; k++) {
    // Not -1: the program is parked on a timer, so the kernel asks to be run
    // again when it expires.
    H.run(now);
    shot.push(H.rows(H.screen()).join("\n"));
    now += k ? 12.5 : 1001;
}

const got = shot.map((f, k) => `--- frame ${k}\n${f}`).join("\n") + "\n";
mkdirSync(dirname(LOG), { recursive: true });
writeFileSync(LOG, got);

const fail = (msg) => {
    console.error(`asciifluid: ${msg}`);
    console.error(`asciifluid: the frames are ${LOG}`);
    process.exit(1);
};

// ------------------------------------------------------------- assertions

// 1. It painted something, and the fluid moved.
if (shot[0].trim() === "")
    fail("the first frame is blank");
if (new Set(shot).size !== shot.length)
    fail("two frames are identical, so the fluid did not move");

// 2. Every frame, byte for byte, against upstream's own.
const golden = readFileSync(join(HERE, "frames.log"), "utf8");
if (got !== golden) {
    const a = got.split("\n"), b = golden.split("\n");
    for (let i = 0; i < Math.max(a.length, b.length); i++)
        if (a[i] !== b[i])
            fail(`line ${i + 1} is ${JSON.stringify(a[i])}, ` +
                 `expected ${JSON.stringify(b[i])}`);
    fail("the frames differ in length alone");
}

console.log(`asciifluid: ${want} frames, upstream's own`);
