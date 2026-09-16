// Unicode in full: unicodedata, str by Unicode's rules, the codecs, and the
// source text the lexer reads.
//
// Three halves. The cases under test/unicode/ are programs that print, run by
// CPython and by this interpreter and compared byte for byte, with CPython's
// own codecs.py and encodings package planted beside them. What a golden
// cannot hold is asserted here: the bytes a lone surrogate becomes on stdout
// and on stderr. And --full runs what is too slow for every run: every
// codepoint's properties against the host's CPython, block by block, and
// Unicode's own NormalizationTest.txt, which lives under tmp/ucd/.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

import { H, boot, put, run, script, ok, die, same, against_cpython } from "./pylib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const LIB = join(HERE, "..", "lib");
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));
const full = process.argv.includes("--full");

await boot("pyunicode");

// The library, where sys.path[0] finds it.
function plant(at) {
    for (const e of readdirSync(at)) {
        const p = join(at, e);
        if (statSync(p).isDirectory()) plant(p);
        else if (p.endsWith(".py")) put("/tmp/" + relative(LIB, p), readFileSync(p));
    }
}
plant(LIB);

let bad = 0;
function check(what, got, want) {
    if (!same(what, got, want)) bad++;
}

// ---------------------------------------------------------- against CPython

const { bad: differ, ran, lines } = against_cpython(join(HERE, "unicode"), only);
bad += differ;
if (!ran) die(only.length ? "no case matched" : "no cases under test/unicode/");

// ------------------------------------------------------------ the streams

// UTF-8 mode: stdout escapes what surrogateescape can and refuses the rest at
// the write, and stderr writes a backslash escape.
function octets(path) {
    const b = H.store.files.get(path);
    return b ? Array.from(b, (x) => x.toString(16).padStart(2, "0")).join(" ") : "";
}

if (!only.length) {
    const r = script("import sys\n" +
        "print(sys.stdout.errors, sys.stderr.errors)\n" +
        "print('a\\udc80b\\udcff')\n" +
        "print('x\\ud800y\\udc80', file=sys.stderr)\n" +
        "try:\n" +
        "    print('z\\ud800')\n" +
        "except UnicodeEncodeError as e:\n" +
        "    print(e.start, e.end, e.reason)\n");
    check("the streams' error handlers", r.out.split("\n")[0], "surrogateescape backslashreplace");
    check("stdout escapes a surrogate to its byte", octets("/tmp/o").split(" 0a ")[1],
          "61 80 62 ff");
    check("stdout refuses what it cannot escape", r.out.split("\n")[2], "1 2 surrogates not allowed");
    check("stderr writes the escape", r.err, "x\\ud800y\\udc80\n");

    // A module whose cookie names a codec written in Python is decoded by it.
    put("/tmp/cp.py", new Uint8Array([
        ...new TextEncoder().encode("# -*- coding: cp1252 -*-\nX = '"), 0x80, 0x99,
        ...new TextEncoder().encode("'\n")]));
    put("/tmp/bad8.py", new Uint8Array([
        ...new TextEncoder().encode("X = '"), 0xff, ...new TextEncoder().encode("'\n")]));
    const m = script("import cp\nprint(ascii(cp.X))\n" +
        "try:\n    import bad8\nexcept SyntaxError as e:\n" +
        "    print(e.msg[:30], e.lineno, e.offset, e.filename, ascii(e.text))\n");
    check("a module in cp1252, and one that is not UTF-8", m.out + m.err,
          "'\\u20ac\\u2122'\n" +
          "Non-UTF-8 code starting with ' 1 6 /tmp/bad8.py \"X = '\\ufffd'\\n\"\n");
}

// ------------------------------------------------------------ --full

function host(args, input) {
    const py = process.env.PYTHON || "python3";
    const r = spawnSync(py, args, { input, maxBuffer: 1 << 30 });
    if (r.status !== 0) die(`${py} failed: ${r.stderr}`);
    return r.stdout.toString();
}

if (full) {
    // Every property of every codepoint, as a digest per block of 256.
    const digest = readFileSync(join(HERE, "unicode", "full", "digest.py"));
    const want = host(["-"], digest);
    const r = script(digest);
    check("every codepoint against the host's CPython", r.out + r.err, want);

    // Unicode's conformance file for the four forms.
    const ver = host(["-c", "import unicodedata; print(unicodedata.unidata_version)"]).trim();
    const file = join(HERE, "..", "tmp", "ucd", ver, "NormalizationTest.txt");
    if (!existsSync(file)) die(`no ${file} -- download it from unicode.org`);
    const rows = [];
    for (const line of readFileSync(file, "utf8").split("\n")) {
        const t = line.split("#")[0].trim();
        if (!t || t.startsWith("@")) continue;
        rows.push(t.split(";").slice(0, 5).map((c) =>
            c.trim().split(/\s+/).map((x) => "\\U" + x.padStart(8, "0")).join("")));
    }
    put("/tmp/normdata.py", "ROWS = [\n" +
        rows.map((r) => "    (" + r.map((c) => `"${c}"`).join(", ") + "),\n").join("") + "]\n");
    const n = script(readFileSync(join(HERE, "unicode", "full", "normtest.py")));
    check("NormalizationTest.txt", n.out + n.err, `${rows.length} rows 0 bad\n`);
}

if (bad) {
    console.error(`\npyunicode: ${bad} checks failed`);
    process.exit(1);
}
ok(`${ran} cases, ${lines} lines identical to CPython's, and to themselves under a collector ` +
   `that never waits; the streams${full ? "; every codepoint, and NormalizationTest.txt" : ""}`);
