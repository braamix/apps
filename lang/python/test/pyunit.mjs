// The shims, before anything stands on them.
//
// CPython's tests are not self-contained the way MicroPython's are: every one
// of them imports unittest and most import test.support, and the real unittest
// pulls in asyncio, logging, argparse and inspect. So shim/ carries a unittest
// and a test.support of our own, and pycases.mjs plants them beside every
// case. A shim that answered wrongly would turn a wrong answer into a pass
// everywhere at once, so it is measured here first.
//
// shim/selfcheck.py produces one of every outcome on purpose -- ok, fail,
// error, skip and an expected failure -- so the summary line the harness reads
// has each of its columns exercised.

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
const got = (r.out + r.err).replace(/0x[0-9a-fA-F]+/g, "0xX");

const exp = join(SHIM, "selfcheck.res");
if (opt.bless) {
    writeFileSync(exp, got);
    console.error("pyunit: blessed selfcheck.res");
    process.exit(0);
}
if (!existsSync(exp)) die(`no golden at ${exp} — run with --bless`);
if (!same("selfcheck.py", got, readFileSync(exp, "utf8"))) process.exit(1);

const m = /^--- ran (\d+) ok (\d+) fail (\d+) error (\d+) skip (\d+) ---$/m.exec(got);
if (!m) die("no summary line — unittest.main() did not finish");
const [, ran, okc, fail, error, skip] = m.map(Number);

// The three that are meant to fail live in one class, and every other test in
// the file must pass. Counting them here as well as comparing the golden says
// the shim's own arithmetic is right, not just unchanged.
const deliberate = (got.match(/^(fail|error) Outcomes\./gm) || []).length;
if (fail + error !== deliberate)
    die(`${fail} failures and ${error} errors, but ${deliberate} are deliberate`);
if (okc + fail + error + skip !== ran)
    die(`the summary does not add up: ${okc}+${fail}+${error}+${skip} != ${ran}`);

ok(`unittest and test.support: ${ran} shim tests, ${okc} pass, ${skip} skip, ` +
   `${deliberate} fail on purpose`);
