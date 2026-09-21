import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { APPS, boot, die, frame, ok, start, tick } from "./acl.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const LOG = join(APPS, "build/games/asciiclock/frames.log");

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

if (shot[shot.length - 1].trim() === "") fail("the last frame is blank");
if (new Set(shot).size < 2) fail("the background did not move");

const golden = readFileSync(join(HERE, "frames.log"), "utf8");
if (got !== golden) {
    const a = got.split("\n"), b = golden.split("\n");
    for (let i = 0; i < Math.max(a.length, b.length); i++)
        if (a[i] !== b[i])
            fail(`line ${i + 1} is ${JSON.stringify(a[i])}, expected ${JSON.stringify(b[i])}`);
    fail("the frames differ in length alone");
}

ok(`${WANT} frames, against the golden`);
