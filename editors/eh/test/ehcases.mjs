// Upstream's own functional tests, run against the port.
//
// The keystroke scripts are upstream's, extracted from its test/Makefile; the
// file goldens are upstream's bytes; the screen goldens were replayed out of
// upstream's own terminfo traces (test/extract.py). Nothing here was written by
// looking at what this port happens to draw.

import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, put, rm, keys_watching, tick, image, submit, settle, chdir, H, ok } from "./ehlib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const CASES = JSON.parse(readFileSync(join(HERE, "cases.json"), "utf8"));

const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

// Cases that still differ, and why. Listed rather than dropped: a regression
// anywhere else still fails the run, and one of these starting to pass is
// reported too. See README.md, "Known differences".
const KNOWN = {
    // Fixed in braam-core's rune_lower/rune_upper; this passes from the first
    // SDK that carries it, and the runner says so when it does.
    flip2: "the SDK's case mapping stops at Greek, and U+1F0F is Greek Extended",
};

await boot("ehcases");

// Upstream ran in its own directory and the status line shows the path as
// given, so the cases keep their relative names and this is where they live.
submit("cd /tmp");
tick(2);
chdir("/tmp");
const AT = (p) => "/tmp/" + p;

const dec = new TextDecoder();
const bytes = (s) => Uint8Array.from(s, (c) => c.charCodeAt(0) & 0xff);

let ran = 0;
const failed = [];
const attempted = new Set();

function report(c, what, got, want) {
    if (!failed.includes(c.name)) failed.push(c.name);
    if (process.env.EH_QUIET) return;
    console.error(`\n${c.name}: ${what}`);
    const g = String(got).split("\n");
    const w = String(want).split("\n");
    let shown = 0;
    for (let i = 0; i < Math.max(g.length, w.length) && shown < 6; i++) {
        const a = (g[i] ?? "").replace(/\s+$/, "");
        const b = (w[i] ?? "").replace(/\s+$/, "");
        if (a === b) continue;
        console.error(`  ${i} got  ${JSON.stringify(a)}`);
        console.error(`  ${i} want ${JSON.stringify(b)}`);
        shown++;
    }
}

for (const c of CASES) {
    if (only.length && !only.includes(c.name)) continue;

    // A fresh /tmp for every case: upstream's harness removed a.txt and a.out
    // before each one.
    for (const p of ["a.txt", "b.txt"]) rm(AT(p));
    for (const s of c.setup) put(AT(s.path), bytes(s.text));

    submit(["eh", ...c.argv].join(" "));
    tick(3);
    const last = keys_watching(c.script, image);
    tick(3);

    ran++;
    attempted.add(c.name);

    if (c.image) {
        // The goldens are runes, as the grid is: the replayer decoded the
        // trace's UTF-8. The data files below are raw bytes and stay latin1.
        let want = readFileSync(join(HERE, "golden", c.name + ".img"), "utf8");
        let got = last;
        // `mask: false` says the trace could not record what the port draws:
        // textterm has no `rev`, so an A_REVERSE cell carries no attribute in
        // the golden. Assert the text half alone. See test/extract.py.
        if (c.mask === false) {
            want = want.split("\n---\n")[0];
            got = got.split("\n---\n")[0];
        }
        // Trailing blank rows and the mask's trailing dots are not content.
        const trim = (s) => s.replace(/[ \t]+$/gm, "").replace(/\n+$/, "");
        if (trim(got) !== trim(want)) report(c, "screen", got, want);
    }

    if (c.file) {
        const p = join(HERE, "data", c.name + ".txt");
        const want = existsSync(p) ? readFileSync(p, "latin1") : "";
        const raw = H.store.files.get(AT(c.file));
        const got = raw === undefined ? "" : Buffer.from(raw).toString("latin1");
        if (got !== want) report(c, "file", JSON.stringify(got), JSON.stringify(want));
    }

    // The next case cannot be submitted while this one still holds the screen,
    // and a script that failed part way may have left the editor up.
    if (!settle()) {
        if (!failed.includes(c.name)) failed.push(c.name);
        // The editor died or is wedged; a fresh submit needs a live shell.
        for (let i = 0; i < 20 && !settle(); i++) tick(4);
    }
}

const unexpected = failed.filter((n) => !(n in KNOWN));
const fixed = Object.keys(KNOWN).filter((n) => !failed.includes(n) && attempted.has(n));

if (unexpected.length) {
    console.error(`\nehcases: ${unexpected.length} of ${ran} failed: ${unexpected.join(" ")}`);
    process.exit(1);
}
if (fixed.length) {
    console.error(`\nehcases: known failures now pass, drop them from KNOWN: ${fixed.join(" ")}`);
    process.exit(1);
}
ok(`${ran - failed.length} of upstream's ${ran} cases; ${failed.length} known differences`);
