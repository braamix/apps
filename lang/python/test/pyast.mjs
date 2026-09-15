// The parser, measured against CPython's. Every golden under lex/ was
// written by tools/mkast.py out of CPython's own tokenize module, so a
// difference here is a difference from CPython -- position, value or all.

import { readdirSync, readFileSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, run, ok, die, same, opt } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const DIR = join(HERE, "ast");
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pyast");

let bad = 0;
let ran = 0;

for (const name of readdirSync(DIR).filter((f) => f.endsWith(".py")).sort()) {
    if (only.length && !only.includes(name)) continue;
    const source = readFileSync(join(DIR, name), "utf8");
    put("/tmp/c.py", source);
    const r = run("--dump-ast /tmp/c.py");
    ran++;

    // A name ending in _err is a source the lexer must refuse; its golden is
    // the tokens it managed plus the complaint, so the message is pinned too.
    const wanted = name.endsWith("_err.py") ? r.out + r.err : r.out;
    const status = name.endsWith("_err.py") ? 1 : 0;
    if (r.status !== status) {
        console.error(`${name}: exited ${r.status}, want ${status}\n${r.err}`);
        bad++;
        continue;
    }

    const exp = join(DIR, name + ".exp");
    if (opt.bless) {
        writeFileSync(exp, wanted);
        continue;
    }
    if (!existsSync(exp)) die(`no golden at ${exp} — run tools/mkast.py`);
    if (!same(name, wanted, readFileSync(exp, "utf8"))) bad++;
}

// And every upstream test in the manifest must at least tokenize. They cannot
// run yet, but their source is real Python and the lexer has to swallow it.
let upstream = 0;
if (!only.length) {
    const rows = readFileSync(join(HERE, "manifest.txt"), "utf8").split("\n");
    for (const line of rows) {
        const t = line.trim();
        if (!t || t.startsWith("#")) continue;
        const name = t.split(/\s+/)[1];
        put("/tmp/c.py", readFileSync(join(HERE, "cases", name)));
        const r = run("--dump-ast /tmp/c.py");
        upstream++;
        if (r.status !== 0) {
            console.error(`${name}: ${r.err.trim()}`);
            bad++;
        }
    }
}

if (opt.bless) {
    console.error(`pyast: blessed ${ran} cases`);
    process.exit(0);
}
if (bad) {
    console.error(`pyast: ${bad} of ${ran} cases differ`);
    process.exit(1);
}
ok(`${ran} sources node for node, ${upstream} upstream tests parsed`);
