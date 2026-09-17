// Coroutines and async generators, measured against CPython the strongest way
// there is: the same program run by both, compared byte for byte.
//
// MicroPython's async_* family (runcases.mjs) says what the language does.
// These say what this implementation had to get right on top of that: every
// way an await is refused, what asend, athrow and aclose do when stepped by
// hand, where each async statement may be written, and a scheduler written in
// Python driving a few dozen tasks through it all.
//
// Under `make test STRESS=1` everything is run twice, the second time
// collecting at every allocation. An awaitable holds a continuation across a
// call into Python, and an async generator's state lives in three objects at
// once, so a missing Root in any of them would be invisible otherwise.

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, ok, die, against_cpython, under_gc } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pycoro");
const { bad, ran, lines } = against_cpython(join(HERE, "coro"), only);

if (!ran) die(only.length ? "no case matched" : "no cases under test/coro/");
if (bad) {
    console.error(`\npycoro: ${bad} of ${ran} differ from CPython`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's${under_gc}`);
