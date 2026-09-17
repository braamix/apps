// The library, first wave: the cases under test/stdlib/, each a program over
// CPython's own modules in lib/, against the CPython those modules come from
// -- the 3.16 build of the clone, since frozendict and sentinel are its own.
// tools/pyref.py says which interpreter wrote each golden.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, ok, die, against_cpython, under_gc } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pystdlib");

const { bad, ran, lines } = against_cpython(join(HERE, "stdlib"), only);
if (!ran) die(only.length ? "no case matched" : "no cases under test/stdlib/");
if (bad) {
    console.error(`\npystdlib: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython 3.16's${under_gc}`);
