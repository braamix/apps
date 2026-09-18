// MicroPython's tests, one boot for all of them. A case is a .py copied byte
// for byte from upstream with its expected output in a .exp beside it;
// manifest.txt says which are expected to pass. A `fail` row is listed rather
// than dropped, so both a regression and an unpromoted fix are caught.
// tools/mkexp.py writes the copy and the row.

import { readFileSync, existsSync, readdirSync } from "node:fs";
import { join, dirname, relative } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, run, ok, die } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));

// A case that imports needs the modules it imports. Everything beside it goes
// into /tmp, which is where the test itself is planted and therefore what
// sys.path[0] names. Done once per directory: basics/ has no support files and
// planting its 250 tests for each of them would be the whole of the run.
const planted = new Set();

function plant_support(dir) {
    if (dir === "basics" || planted.has(dir)) return;
    planted.add(dir);
    const root = join(HERE, "cases", dir);
    const walk = (at) => {
        for (const e of readdirSync(at, { withFileTypes: true })) {
            const p = join(at, e.name);
            if (e.isDirectory()) walk(p);
            else if (!p.endsWith(".exp")) put("/tmp/" + relative(root, p), readFileSync(p));
        }
    };
    if (existsSync(root)) walk(root);
}
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

// state case exp commit upstream — manifest.txt has the column notes.
function manifest() {
    const path = join(HERE, "manifest.txt");
    if (!existsSync(path)) die(`no manifest at ${path}`);
    const rows = [];
    let n = 0;
    for (const line of readFileSync(path, "utf8").split("\n")) {
        n++;
        const t = line.trim();
        if (!t || t.startsWith("#")) continue;
        const f = t.split(/\s+/);
        if (f.length !== 5) die(`manifest.txt line ${n}: want 5 fields, got ${f.length}`);
        if (f[0] !== "pass" && f[0] !== "fail")
            die(`manifest.txt line ${n}: state is \`${f[0]}\`, want pass or fail`);
        rows.push({ state: f[0], name: f[1], exp: f[2], commit: f[3], upstream: f[4] });
    }
    return rows;
}

const CASES = manifest().filter((c) => !only.length || only.includes(c.name));

if (!CASES.length) {
    // An empty manifest still proves the harness, the plant and the pipe.
    await boot("runcases");
    run("-V");
    ok(only.length ? "no case matched" : "no cases yet — see test/manifest.txt");
    process.exit(0);
}

await boot("runcases");

const failed = [];
for (const c of CASES) {
    const src = join(HERE, "cases", c.name);
    const exp = src + ".exp";
    if (!existsSync(src)) die(`${c.name}: no copy at ${src} — run tools/mkexp.py`);
    if (!existsSync(exp)) die(`${c.name}: no expected output at ${exp}`);

    plant_support(dirname(c.name));
    put("/tmp/c.py", readFileSync(src));
    const r = run("/tmp/c.py");
    const want = readFileSync(exp, "utf8");
    const got = r.out + r.err;
    if (got === want) continue;

    failed.push(c.name);
    if (c.state === "pass" && !process.env.PY_QUIET) {
        const a = want.split("\n"), b = got.split("\n");
        console.error(`\n${c.name}: (${c.upstream} @ ${c.commit})`);
        for (let i = 0, shown = 0; i < Math.max(a.length, b.length) && shown < 6; i++)
            if (a[i] !== b[i]) {
                console.error(`  ${i + 1} want ${JSON.stringify(a[i])}`);
                console.error(`  ${i + 1} got  ${JSON.stringify(b[i])}`);
                shown++;
            }
    }
}

const broke = CASES.filter((c) => c.state === "pass" && failed.includes(c.name));
const fixed = CASES.filter((c) => c.state === "fail" && !failed.includes(c.name));

if (broke.length) {
    console.error(`\nruncases: ${broke.length} of ${CASES.length} regressed: ` +
                  broke.map((c) => c.name).join(" "));
    process.exit(1);
}
if (fixed.length) {
    console.error(`\nruncases: these now pass, mark them pass in manifest.txt: ` +
                  fixed.map((c) => c.name).join(" "));
    process.exit(1);
}
const passing = CASES.length - failed.length;
ok(`${passing} of ${CASES.length} upstream cases; ${failed.length} not reached yet`);
