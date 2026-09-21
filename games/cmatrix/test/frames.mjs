// Sixteen frames of a seeded rain, against the golden beside this file.
//
// The run is deterministic: the dice are pinned by CMATRIX_SEED, malloc is
// zeroed, and nothing reads the clock, so the assertion is the whole screen,
// byte for byte.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { APPS, boot, die, frame, ok, row, start, tick } from "./cmlib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const LOG = join(APPS, "build/games/cmatrix/frames.log");

await boot("frames");
start();

const WANT = 16;
const shot = [];
for (let k = 0; k < WANT; k++) {
    tick();
    shot.push(frame());
}

const got = shot.map((f, k) => `--- frame ${k}\n${f}`).join("\n") + "\n";
mkdirSync(dirname(LOG), { recursive: true });
writeFileSync(LOG, got);

const fail = (msg) => {
    console.error(`frames: ${msg}`);
    console.error(`frames: the frames are ${LOG}`);
    process.exit(1);
};

// 1. It painted, and the rain moved.
if (shot[shot.length - 1].trim() === "") fail("the last frame is blank");
if (new Set(shot).size < 2) fail("the rain did not move");

// 2. The rain occupies even columns; odd ones stay blank, which is j += 2.
let even = 0, odd = 0;
const last = shot[shot.length - 1].split("\n");
for (const line of last) {
    for (let x = 0; x < line.length; x++) {
        if (line[x] === " ") continue;
        if (x % 2 === 0) even++;
        else odd++;
    }
}
if (even === 0) fail(`no glyphs in even columns: ${row(0)}`);
if (odd !== 0) fail(`glyphs in odd columns, which the rain skips: ${last.join("|")}`);

// 3. Every frame, byte for byte.
const golden = readFileSync(join(HERE, "frames.log"), "utf8");
if (got !== golden) {
    const a = got.split("\n"), b = golden.split("\n");
    for (let i = 0; i < Math.max(a.length, b.length); i++)
        if (a[i] !== b[i])
            fail(`line ${i + 1} is ${JSON.stringify(a[i])}, expected ${JSON.stringify(b[i])}`);
    fail("the frames differ in length alone");
}

ok(`${WANT} frames, against the golden`);
