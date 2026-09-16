// Formatting, measured against CPython the strongest way there is: the same
// program run by both, compared byte for byte.
//
// These cases are ours rather than an upstream's, because what they have to
// cover is a grammar and not a feature -- every corner of the spec
// mini-language, of `%`, of str.format's fields and of an f-string's body.
// tools/mkfmt.py writes each golden by running the case under the host's
// CPython, which is why a case may use nothing this interpreter has not got.
//
// Everything is run twice, the second time collecting at every allocation:
// the format engine holds a plan, an output list and a spec buffer across
// calls into Python, and a missing Root there would be invisible otherwise.

import { readdirSync, readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, run, ok, die, same } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const DIR = join(HERE, "format");
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pyformat");

let bad = 0;
let ran = 0;
let lines = 0;

for (const name of readdirSync(DIR).filter((f) => f.endsWith(".py")).sort()) {
    if (only.length && !only.includes(name)) continue;
    const exp = join(DIR, name + ".exp");
    if (!existsSync(exp)) die(`${name}: no golden — run tools/mkfmt.py test/format/${name}`);

    put("/tmp/c.py", readFileSync(join(DIR, name)));
    const want = readFileSync(exp, "utf8");
    const r = run("/tmp/c.py");
    ran++;
    lines += want.split("\n").length - 1;
    if (!same(name, r.out + r.err, want)) {
        bad++;
        continue;
    }

    const under = run("/tmp/c.py", null, "PY_GC_STRESS=1");
    if (!same(`${name} under gc stress`, under.out + under.err, want)) bad++;
}

if (!ran) die(only.length ? "no case matched" : `no cases under ${DIR}`);
if (bad) {
    console.error(`\npyformat: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's, and to themselves under ` +
   `a collector that never waits`);
