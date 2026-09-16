// Lazy imports (PEP 810): the cases under test/lazy/, with the modules they
// import planted beside them, against CPython -- 3.16, the first to have the
// statement; tools/pyref.py says which interpreter wrote each golden.

import { readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, ok, die, against_cpython } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pylazy");

// Everything under lazy/mods/ goes to /tmp/mods/, where the case finds it.
const MODS = join(HERE, "lazy", "mods");
function plant(dir) {
    for (const f of readdirSync(dir)) {
        const p = join(dir, f);
        if (statSync(p).isDirectory()) plant(p);
        else if (f.endsWith(".py")) put("/tmp/mods/" + relative(MODS, p), readFileSync(p));
    }
}
plant(MODS);

const { bad, ran, lines } = against_cpython(join(HERE, "lazy"), only);
if (!ran) die(only.length ? "no case matched" : "no cases under test/lazy/");
if (bad) {
    console.error(`\npylazy: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython 3.16's, and to themselves under ` +
   `a collector that never waits`);
