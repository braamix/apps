// unittest and the shims, before anything stands on them.
//
// CPython's tests are not self-contained the way MicroPython's are: every one
// of them imports unittest and most import test.support. The unittest is
// CPython's own as of phase 27; `test.support` is still shim/'s, and
// pycases.mjs plants it beside every case. Something that answered wrongly
// there would turn a wrong answer into a pass everywhere at once, so it is
// measured here first.
//
// shim/selfcheck.py produces one of every outcome on purpose -- ok, fail,
// error, skip and an expected failure -- so every column of the report the
// harness reads is exercised.

import { readFileSync, writeFileSync, existsSync, readdirSync } from "node:fs";
import { join, dirname, relative } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, run, ok, die, same, opt } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const SHIM = join(HERE, "shim");

await boot("pyunit");

const walk = (at) => {
    for (const e of readdirSync(at, { withFileTypes: true })) {
        const p = join(at, e.name);
        if (e.isDirectory()) walk(p);
        else if (p.endsWith(".py")) put("/tmp/" + relative(SHIM, p), readFileSync(p));
    }
};
walk(SHIM);

const r = run("/tmp/selfcheck.py");
const got = (r.out + r.err)
    .replace(/0x[0-9a-fA-F]+/g, "0xX")
    .replace(/ in [0-9.]+s$/gm, " in Ns");

const exp = join(SHIM, "selfcheck.res");
if (opt.bless) {
    writeFileSync(exp, got);
    console.error("pyunit: blessed selfcheck.res");
    process.exit(0);
}
if (!existsSync(exp)) die(`no golden at ${exp} — run with --bless`);
if (!same("selfcheck.py", got, readFileSync(exp, "utf8"))) process.exit(1);

const m = /^Ran (\d+) tests? in /m.exec(got);
if (!m) die("no summary line — unittest.main() did not finish");
const ran = Number(m[1]);

// The outcomes unittest reported, off the line it ends with.
const tally = {};
const f = /^FAILED \((.*)\)$/m.exec(got);
if (f)
    for (const part of f[1].split(", ")) {
        const [kind, n] = part.split("=");
        tally[kind] = Number(n);
    }
const fail = tally.failures || 0;
const error = tally.errors || 0;
const skip = tally.skipped || 0;

// The ones meant to fail live in one class, and every other test in the file
// must pass. Counting them here as well as comparing the golden says the
// arithmetic is right, not just unchanged.
const deliberate = (got.match(/^(FAIL|ERROR): \w+ \(__main__\.Outcomes\./gm) || []).length;
if (fail + error !== deliberate)
    die(`${fail} failures and ${error} errors, but ${deliberate} are deliberate`);

ok(`unittest and test.support: ${ran} tests, ${ran - fail - error} pass, ${skip} skip, ` +
   `${deliberate} fail on purpose`);
