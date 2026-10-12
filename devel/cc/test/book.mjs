// The book's programs ("Writing a C Compiler"), compiled on Braam and run,
// with main's result after the output (suites.mjs).

import { run_suites } from "./suites.mjs";
import { readdirSync } from "node:fs";
import { TREE } from "./ccfix.mjs";
import { join } from "node:path";

const suites = readdirSync(join(TREE, "test/programs"))
    .filter((d) => /^chapter|^signed_char/.test(d)).sort();

// A main renamed is an ordinary function, and loses main's implicit return 0.
const IMPLICIT = "relies on main's implicit return 0";
await run_suites("book", suites, true, {
    Chapter9_HelloWorld: IMPLICIT,
    Chapter10_StaticRecursiveCall: IMPLICIT,
    Chapter16_PointerOperations: IMPLICIT,
});
