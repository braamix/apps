// Generators, measured against CPython the strongest way there is: the same
// program run by both, compared byte for byte.
//
// MicroPython's generator* and gen_yield_from* families (runcases.mjs) say
// what the language does. These say what this implementation had to get right
// on top of that: which builtin parks on a generator, what the frame keeps
// across a suspension, and where a throw lands in a chain of delegations.
//
// Everything is run twice, the second time collecting at every allocation. A
// parked frame is reachable only through the generator that holds it, and a
// delegation holds a continuation across a call into Python, so a missing Root
// in either would be invisible otherwise.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, ok, die, against_cpython } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pygen");
const { bad, ran, lines } = against_cpython(join(HERE, "gen"), only);

if (!ran) die(only.length ? "no case matched" : "no cases under test/gen/");
if (bad) {
    console.error(`\npygen: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's, and to themselves under ` +
   `a collector that never waits`);
