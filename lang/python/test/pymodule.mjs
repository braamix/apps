// The modules written in C++, measured against CPython the strongest way
// there is: the same program run by both, compared byte for byte.
//
// These cases are ours rather than an upstream's, because what they cover is
// a set of modules and not a feature, and because an upstream's own tests all
// import the library that stands on them -- which is phase 20. What a case may
// print is therefore what every implementation agrees on: a number CPython and
// musl round alike, a struct_time for a fixed instant, a Mersenne Twister
// seeded by hand. A clock reading, an object's size and a collection count are
// this interpreter's own and are printed only as types.
//
// Under `make test STRESS=1` everything is run twice, the second time
// collecting at every allocation: every module here holds objects across a
// call back into Python -- a cache, a deque being extended, an eager pass's
// output list -- and a missing Root there would be invisible otherwise.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, ok, die, against_cpython, under_gc } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pymodule");
const { bad, ran, lines } = against_cpython(join(HERE, "module"), only);

if (!ran) die(only.length ? "no case matched" : "no cases under test/module/");
if (bad) {
    console.error(`\npymodule: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's${under_gc}`);
