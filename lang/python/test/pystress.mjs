// Every passing case run twice: once plainly, once with PY_GC_STRESS, which
// collects at every allocation.
//
// C++ holding an object across an allocation must pin it. A missing Root is
// invisible until a collection lands in the gap, so running the whole manifest
// with every gap closed turns it into a wrong answer. Nine were found that
// way: four in the `with` statement, the rest in the special-method path.

import { readFileSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, run, ok, die } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));

const rows = readFileSync(join(HERE, "manifest.txt"), "utf8")
    .split("\n")
    .map((l) => l.trim())
    .filter((l) => l && !l.startsWith("#"))
    .map((l) => l.split(/\s+/))
    .filter((f) => f[0] === "pass")
    .map((f) => f[1]);

await boot("pystress");

const differ = [];
for (const name of rows) {
    const src = join(HERE, "cases", name);
    if (!existsSync(src)) die(`${name}: no copy at ${src}`);
    put("/tmp/c.py", readFileSync(src));
    const plain = run("/tmp/c.py");
    const under = run("/tmp/c.py", null, "PY_GC_STRESS=1");
    const a = plain.out + plain.err;
    const b = under.out + under.err;
    if (a === b) continue;

    differ.push(name);
    const x = a.split("\n"), y = b.split("\n");
    console.error(`\n${name}:`);
    for (let i = 0, n = 0; i < Math.max(x.length, y.length) && n < 4; i++)
        if (x[i] !== y[i]) {
            console.error(`  ${i + 1} plain  ${JSON.stringify(x[i])}`);
            console.error(`  ${i + 1} stress ${JSON.stringify(y[i])}`);
            n++;
        }
}

if (differ.length)
    die(`${differ.length} of ${rows.length} answer differently under a collector ` +
        `that never waits: ${differ.join(" ")}`);
ok(`${rows.length} cases, unchanged by collecting at every allocation`);
