// Formatting, measured against CPython the strongest way there is: the same
// program run by both, compared byte for byte.
//
// These cases are ours rather than an upstream's, because what they have to
// cover is a grammar and not a feature -- every corner of the spec
// mini-language, of `%`, of str.format's fields and of an f-string's body.
//
// Everything is run twice, the second time collecting at every allocation:
// the format engine holds a plan, an output list and a spec buffer across
// calls into Python, and a missing Root there would be invisible otherwise.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, ok, die, against_cpython } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pyformat");
const { bad, ran, lines } = against_cpython(join(HERE, "format"), only);

if (!ran) die(only.length ? "no case matched" : "no cases under test/format/");
if (bad) {
    console.error(`\npyformat: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's, and to themselves under ` +
   `a collector that never waits`);
