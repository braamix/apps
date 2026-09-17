// compile, eval, exec, the namespaces and the attributes a function and a code
// object answer to -- against CPython, the same program run by both.
//
// These all run Python from inside a builtin, and a namespace given to exec()
// is held across that call, so the second run `make test STRESS=1` makes,
// under a collector that never waits, is what says the Roots are right.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, ok, die, against_cpython, under_gc } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pyexec");
const { bad, ran, lines } = against_cpython(join(HERE, "exec"), only);

if (!ran) die(only.length ? "no case matched" : "no cases under test/exec/");
if (bad) {
    console.error(`\npyexec: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's${under_gc}`);
