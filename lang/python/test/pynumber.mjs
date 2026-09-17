// The number tower, measured against CPython the same way the formatting is:
// the same program run by both, compared byte for byte.
//
// What these cover is arithmetic that has one right answer and many nearly
// right ones -- a long division, a quotient rounded once rather than three
// times, a bitwise operator over infinite two's complement, a repr that keeps
// a negative zero. Guessing at any of those would pass a hand-written test.
//
// Under PY_GC_STRESS as well: bigint.cpp builds a magnitude in plain C++ and
// only the last step allocates, and this is what says that rule is kept.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, ok, die, against_cpython, under_gc } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pynumber");
const { bad, ran, lines } = against_cpython(join(HERE, "number"), only);

if (!ran) die(only.length ? "no case matched" : "no cases under test/number/");
if (bad) {
    console.error(`\npynumber: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's${under_gc}`);
