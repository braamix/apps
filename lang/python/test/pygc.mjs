// `python --selftest`: the object heap and the collector, checked from inside
// the program. There is no Python to run yet, so this is what phase 1 asserts.

import { boot, run, ok, die } from "./pylib.mjs";

await boot("pygc");

const r = run("--selftest");

if (r.status !== 0) {
    console.error(r.err || r.out);
    die(`--selftest exited ${r.status}`);
}
if (r.err !== "") die(`--selftest wrote to stderr: ${JSON.stringify(r.err)}`);

const lines = r.out.split("\n");
if (lines.pop() !== "") die("the output does not end in a newline");

const tail = lines.pop();
const m = /^selftest: (\d+) of (\d+)$/.exec(tail ?? "");
if (!m) die(`the last line is not the count: ${JSON.stringify(tail)}`);
if (m[1] !== m[2]) die(`${m[1]} of ${m[2]} checks passed`);

for (const line of lines) if (!line.startsWith("ok ")) die(`not a pass: ${line}`);
if (lines.length !== Number(m[2]))
    die(`counted ${lines.length} checks, the tally says ${m[2]}`);

// Named so that a check silently disappearing is a failure too.
const WANT = ["value", "strings", "intern", "collect", "root", "tuple", "list",
              "cycle", "deep", "stress", "threshold", "numbers", "floats",
              "compare", "strtext", "reprs", "dict", "set", "errors", "truth",
              "compile", "scopes"];
const got = lines.map((l) => l.slice(3));
for (const w of WANT) if (!got.includes(w)) die(`the ${w} check did not run`);

ok(`${m[1]} checks`);
