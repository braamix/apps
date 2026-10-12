// The book's programs ("Writing a C Compiler"), compiled on Braam and run,
// with main's result after the output (suites.mjs).

import { run_suites } from "./suites.mjs";
import { readdirSync } from "node:fs";
import { TREE } from "./ccfix.mjs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const suites = readdirSync(join(TREE, "test/programs"))
    .filter((d) => /^chapter|^signed_char/.test(d)).sort();

// A main renamed is an ordinary function, and loses main's implicit return 0.
const IMPLICIT = "relies on main's implicit return 0";
export const BOOK = [suites, true, {
    Chapter9_HelloWorld: IMPLICIT,
    Chapter10_StaticRecursiveCall: IMPLICIT,
    Chapter16_PointerOperations: IMPLICIT,
}];

if (resolve(process.argv[1]) === fileURLToPath(import.meta.url))
    await run_suites("book", ...BOOK);
