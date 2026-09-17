// CPython's own tests, one boot for all of them. A case is a .py copied byte
// for byte from Lib/test/ with a .res golden beside it; cpython.txt says which
// run and what they print. tools/mkcpy.py writes the copy and the row, and
// --bless writes the golden.
//
// These are not MicroPython's tests. One of those is a program that prints and
// is compared against CPython's output; one of these imports unittest, builds
// classes, and asserts. So the shims under shim/ are planted beside every case
// and the golden is the listing unittest.main() printed -- a line per test
// method and its outcome, which is a much finer ruler than a whole transcript.
//
// A case that cannot run at all is a `fail` row whose golden is what it
// printed instead, most often the compiler refusing syntax a later phase adds.
// The row stays rather than the file being edited.

import { readFileSync, writeFileSync, existsSync, readdirSync, mkdirSync } from "node:fs";
import { join, dirname, relative } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, run, ok, die, same, opt, shard } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const CASES = join(HERE, "cpython");
const SHIM = join(HERE, "shim");
const MANIFEST = join(HERE, "cpython.txt");

const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

// state case result commit upstream — cpython.txt has the column notes.
function manifest() {
    if (!existsSync(MANIFEST)) die(`no manifest at ${MANIFEST}`);
    const rows = [];
    let n = 0;
    for (const line of readFileSync(MANIFEST, "utf8").split("\n")) {
        n++;
        const t = line.trim();
        if (!t || t.startsWith("#")) continue;
        const f = t.split(/\s+/);
        if (f.length !== 5) die(`cpython.txt line ${n}: want 5 fields, got ${f.length}`);
        if (f[0] !== "pass" && f[0] !== "fail")
            die(`cpython.txt line ${n}: state is \`${f[0]}\`, want pass or fail`);
        rows.push({ state: f[0], name: f[1], result: f[2], commit: f[3], upstream: f[4] });
    }
    return rows;
}

// The whole of shim/ goes into /tmp, which is where a case is planted and
// therefore what sys.path[0] names. Once per boot: it is the same for every
// case, and planting it 10 times would be most of the run.
function plant_shims() {
    const walk = (at) => {
        for (const e of readdirSync(at, { withFileTypes: true })) {
            const p = join(at, e.name);
            if (e.isDirectory()) walk(p);
            else if (p.endsWith(".py")) put("/tmp/" + relative(SHIM, p), readFileSync(p));
        }
    };
    walk(SHIM);
}

// The data files some cases read, under the shims' test package. data.txt
// lists them and where they came from.
function plant_data() {
    const list = join(CASES, "data.txt");
    if (!existsSync(list)) return;
    for (const line of readFileSync(list, "utf8").split("\n")) {
        const t = line.trim();
        if (!t || t.startsWith("#")) continue;
        const path = t.split(/\s+/)[0];
        put("/tmp/test/" + path, readFileSync(join(CASES, "test", path)));
    }
}

// An object's repr carries its address, which moves with the allocation order,
// and os_helper names a temporary after the pid, which moves with how many
// processes the boot has spawned before it. Nothing else in a listing is
// unstable.
function stable(text) {
    return text
        .replace(/0x[0-9a-fA-F]+/g, "0xX")
        .replace(/@test_\d+_tmp/g, "@test_N_tmp")
        .replace(/ in [0-9.]+s$/gm, " in Ns");
}

// What unittest.main() printed, which is the whole of the output: its report
// ends with the line `Ran N tests in ...`. Null when the case never got there.
function listing(text) {
    return /^Ran \d+ tests? in /m.test(text) ? text : null;
}

// A one-word reason for a case that does not run, read off what it printed.
// The golden is the ruler; this is the column a person reads.
const REASONS = [
    [/f-strings /, "fstring"],
    [/complex numbers /, "complex"],
    [/int too large/, "bignum"],
    [/\\N\{\.\.\.\} escapes/, "namedesc"],
    [/invalid unicode escape/, "unicodeesc"],
    [/async is not compiled/, "async"],
    [/SyntaxError/, "syntax"],
    [/(ImportError|ModuleNotFoundError)/, "import"],
];

function reason(text) {
    for (const [re, word] of REASONS) if (re.test(text)) return word;
    const m = /^\w*Error: .*/m.exec(text);
    return m ? "runtime" : "silent";
}

// ok/ran off unittest's own last two lines. A skip counts as a pass, as the
// shim's own tally did.
function counts(text) {
    const m = /^Ran (\d+) tests? in /m.exec(text);
    if (!m) return null;
    let bad = 0;
    const f = /^FAILED \((.*)\)$/m.exec(text);
    if (f)
        for (const part of f[1].split(", ")) {
            const [kind, n] = part.split("=");
            if (kind === "failures" || kind === "errors") bad += Number(n);
        }
    return `${Number(m[1]) - bad}/${m[1]}`;
}

await boot("pycases");
plant_shims();
plant_data();

// --survey runs the whole of the clone under tmp/ and says what stopped each
// file. Not a test -- tmp/ is not committed -- but it is how the next wave is
// chosen, the guess this phase started from having turned out wrong: what
// keeps CPython's tests out is the compiler, not the imports.
if (process.argv.includes("--survey")) {
    const dir = join(HERE, "..", "tmp", "cpython", "Lib", "test");
    if (!existsSync(dir)) die(`no clone at ${dir} — see TODO.md, The two upstreams`);
    const tally = new Map();
    const runs = [];
    let n = 0;
    for (const f of readdirSync(dir).filter((f) => /^test_\w+\.py$/.test(f)).sort()) {
        const src = readFileSync(join(dir, f));
        if (src.length > 400000) continue;      // a run of one is not worth minutes
        n++;
        put("/tmp/s.py", src);
        const r = run("/tmp/s.py");
        const got = stable(r.out + r.err);
        const word = listing(got) === null ? reason(got) : "runs";
        if (word === "runs") runs.push(f);
        tally.set(word, (tally.get(word) ?? 0) + 1);
    }
    for (const [word, count] of [...tally].sort((a, b) => b[1] - a[1]))
        console.log(String(count).padStart(5) + "  " + word);
    console.log(`${n} files under Lib/test/; these run: ${runs.join(" ") || "none"}`);
    process.exit(0);
}

const ROWS = shard(manifest().filter((c) => !only.length || only.includes(c.name)));
if (!ROWS.length) die(only.length ? "no case matched" : "cpython.txt has no rows");

const blessed = [];
const broke = [];
const fixed = [];
let running = 0;
let methods = 0;
let passing = 0;

for (const c of ROWS) {
    const src = join(CASES, c.name);
    if (!existsSync(src)) die(`${c.name}: no copy at ${src} — run tools/mkcpy.py`);

    put("/tmp/" + c.name, readFileSync(src));
    const r = run("/tmp/" + c.name);
    const got = stable(r.out + r.err);
    const fenced = listing(got);
    const state = fenced === null ? "fail" : "pass";
    const result = fenced === null ? reason(got) : counts(fenced);
    if (result === null) die(`${c.name}: it printed a fence with no summary line`);

    const exp = join(CASES, c.name.replace(/\.py$/, ".res"));
    if (opt.bless) {
        writeFileSync(exp, fenced === null ? got : fenced);
        blessed.push({ ...c, state, result });
        continue;
    }

    if (state === "pass") {
        running++;
        const [okc, ran] = result.split("/").map(Number);
        methods += ran;
        passing += okc;
    }

    // A row that started running, or one whose count went up, is forward; any
    // other change -- a lower count, a different reason, a case that stopped
    // running -- has to be looked at before the row is moved.
    if (state !== c.state || result !== c.result) {
        const forward =
            (state === "pass" && c.state === "fail") ||
            (state === "pass" && c.state === "pass" &&
             Number(result.split("/")[0]) > Number(c.result.split("/")[0]));
        (forward ? fixed : broke).push(`${c.name} ${c.result}->${result}`);
        continue;
    }
    if (!existsSync(exp)) die(`${c.name}: no golden at ${exp} — run with --bless`);
    const want = readFileSync(exp, "utf8");
    const mine = fenced === null ? got : fenced;
    if (mine !== want) {
        same(c.name, mine, want);
        broke.push(c.name);
    }
}

if (opt.bless) {
    // The rows carry the state and the counts the run just produced, so the
    // manifest and the goldens cannot disagree.
    const text = readFileSync(MANIFEST, "utf8").split("\n");
    const head = [];
    for (const line of text) {
        const t = line.trim();
        if (t && !t.startsWith("#")) break;
        head.push(line.replace(/\s+$/, ""));
    }
    while (head.length && head[head.length - 1] === "") head.pop();

    const kept = manifest().filter((c) => !blessed.some((b) => b.name === c.name));
    const rows = kept
        .concat(blessed)
        .map((c) => [c.state, c.name, c.result, c.commit, c.upstream])
        .sort((a, b) => (a[1] < b[1] ? -1 : a[1] > b[1] ? 1 : 0));
    const w = [0, 1, 2, 3, 4].map((i) => Math.max(...rows.map((r) => r[i].length)));
    const out = head
        .concat(rows.map((r) => r.map((v, i) => v.padEnd(w[i])).join("  ").replace(/\s+$/, "")))
        .join("\n");
    writeFileSync(MANIFEST, out + "\n");
    console.error(`pycases: blessed ${blessed.length} cases and their rows`);
    process.exit(0);
}

if (broke.length) {
    console.error(`\npycases: ${broke.length} of ${ROWS.length} answer differently and not ` +
                  `for the better: ${broke.join(" ")}`);
    process.exit(1);
}
if (fixed.length) {
    console.error(`\npycases: these moved forward, re-bless cpython.txt: ${fixed.join(" ")}`);
    process.exit(1);
}

const stalled = ROWS.length - running;
ok(`${running} of ${ROWS.length} upstream tests run, ${passing} of ${methods} test methods pass` +
   (stalled ? `; ${stalled} still wait on a later phase` : ""));
