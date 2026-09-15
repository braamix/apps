// The compiler, listing for listing. The bytecode is this implementation's
// own, so there is no CPython to measure it against: every golden under dis/
// was blessed from what this printed, and it is here to catch a change nobody
// meant. A name ending in _err is a source the compiler must refuse.

import { readdirSync, readFileSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, run, ok, die, same, opt } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const DIR = join(HERE, "dis");
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

await boot("pydis");

let bad = 0;
let ran = 0;

for (const name of readdirSync(DIR).filter((f) => f.endsWith(".py")).sort()) {
    if (only.length && !only.includes(name)) continue;
    put("/tmp/c.py", readFileSync(join(DIR, name), "utf8"));
    const r = run("--dis /tmp/c.py");
    ran++;

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
    if (!existsSync(exp)) die(`no golden at ${exp} — run with --bless`);
    if (!same(name, wanted, readFileSync(exp, "utf8"))) bad++;
}

// And every upstream test in the manifest must at least compile. They cannot
// run yet, but a refusal here is a hole in the compiler.
let upstream = 0;
let refused = 0;
if (!only.length) {
    const rows = readFileSync(join(HERE, "manifest.txt"), "utf8").split("\n");
    for (const line of rows) {
        const t = line.trim();
        if (!t || t.startsWith("#")) continue;
        const name = t.split(/\s+/)[1];
        put("/tmp/c.py", readFileSync(join(HERE, "cases", name)));
        const r = run("--dis /tmp/c.py");
        upstream++;
        if (r.status !== 0) {
            console.error(`${name}: ${r.err.trim()}`);
            refused++;
        }
    }
}

if (opt.bless) {
    console.error(`pydis: blessed ${ran} cases`);
    process.exit(0);
}
if (bad || refused) {
    console.error(`pydis: ${bad} of ${ran} listings differ, ${refused} upstream refused`);
    process.exit(1);
}
ok(`${ran} listings, ${upstream} upstream tests compiled`);
