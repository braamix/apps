// Twelve frames of a seeded aquarium, against the golden beside this file.
//
// The run is deterministic: the dice are pinned by ASCIIQUARIUM_SEED and
// nothing reads the clock, so the assertion is the whole screen, byte for byte.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { APPS, boot, die, frame, ok, row, start, tick } from "./aqlib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const LOG = join(APPS, "build/games/asciiquarium/frames.log");

await boot("frames");
start();

const WANT = 12;
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

// 1. It painted, and the aquarium moved.
if (shot[0].trim() === "") fail("the first frame is blank");
if (new Set(shot).size !== shot.length) fail("two frames are identical");

// 2. The scenery is where the spawners put it: four water bands at rows 5 to 8,
// and the castle in the bottom right corner.
// A surface entity crosses these rows, so it is most of the band, not all.
for (let y = 5; y <= 8; y++) {
    const band = row(y).split("").filter((c) => c === "~" || c === "^").length;
    if (band < 30) fail(`row ${y} is not a water band: ${row(y)}`);
}
// Seaweed is in front of the castle, so its base is a run of underscores with
// a stalk or two through it rather than the sprite as written.
if (!row(10).includes("T~~")) fail(`no castle turret on row 10: ${row(10)}`);
if ((row(22).match(/_/g) ?? []).length < 15) fail(`no castle base on row 22: ${row(22)}`);
if (row(23) !== "") fail("the bottom row was drawn on");

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
