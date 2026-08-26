// The conversion tables, against GNU libiconv's answers.
//
// Upstream ships a reference corpus in tests/iconv/ref/: one file per
// encoding, a line per mapping, `<in> = <out>` in hex. It was generated
// against GNU libiconv, so agreeing with it is agreeing with GNU rather than
// with ourselves — which is what makes this worth more than the rest of the
// suite together.
//
// The corpus is not checked in; it lives in the gitignored upstream tree.
// Without it this test says so and passes over, the way a test needing a
// browser would.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/converters/iconv/iconv.wasm"),
    data: join(APPS, "build/converters/iconv/i18n"),
    ref: join(HERE, "../tmp/citrus-iconv/tests/iconv/ref"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

// Where citrus and GNU agree, and the port must too. The single-byte
// encodings exercise every mapper shape: ISO8859-1 is the identity forward and
// a zone/std pair back, KOI8-R is a parallel of two tables, the rest are plain
// std tables. BIG5 and EUC-KR bring in the multibyte stdenc modules.
const EXACT = [
    "ASCII", "ISO8859-1", "ISO8859-2", "ISO8859-3", "ISO8859-4", "ISO8859-5",
    "ISO8859-6", "ISO8859-7", "ISO8859-8", "ISO8859-9", "ISO8859-10",
    "ISO8859-11", "ISO8859-13", "ISO8859-14", "ISO8859-15", "ISO8859-16",
    "ARMSCII-8", "CP1251", "CP866", "PT154", "VISCII", "KOI8-R", "KOI8-U",
    "BIG5", "EUC-KR",
];

// Where citrus and GNU do not agree, mostly over the user-defined and
// vendor-defined rows of the CJK sets. This is upstream's own position, not a
// concession made here: tests/iconv/tablegen/cmp.sh prints "DIFFER" and then
// exits 0 whatever it found, so upstream's suite never fails on these either.
// Reported rather than asserted, so a change in the count is visible.
const INFORMATIONAL = [
    "BIG5-HKSCS", "CP949", "DEC-KANJI", "EUC-CN", "EUC-JP",
    "GB18030", "GB2312", "GBK", "SHIFT_JIS", "UTF-8-MAC",
];

const ENCODINGS = [...EXACT, ...INFORMATIONAL];

if (!existsSync(opt.ref)) {
    console.log("convert skipped: no reference corpus at " + opt.ref);
    process.exit(0);
}
for (const [what, path, how] of [
    ["kernel", opt.kernel, "make -C ../braam-core"],
    ["rootfs", opt.rootfs, "make -C ../braam-core"],
    ["iconv", opt.binary, "make"],
    ["the i18n tree", opt.data, "make"],
]) {
    if (!existsSync(path)) {
        console.error(`convert: no ${what} at ${path} — run \`${how}\``);
        process.exit(1);
    }
}

const H = await import(join(CORE, "test/system/harness.mjs"));

function die(msg) {
    console.error("convert: " + msg);
    process.exit(1);
}

const failures = [];
const differences = [];
let current = "";
function bad(msg) {
    (EXACT.includes(current) ? failures : differences).push(msg);
}

// `0x00C7 = 0x0433` — the left is a byte sequence as a big-endian number, the
// right the codepoint it maps to. A line without a mapping is not listed.
function parseRef(path) {
    const pairs = [];
    for (const line of readFileSync(path, "utf8").split("\n")) {
        const m = /^0x([0-9A-Fa-f]+)\s*=\s*0x([0-9A-Fa-f]+)\s*$/.exec(line);
        if (m) pairs.push([parseInt(m[1], 16), parseInt(m[2], 16)]);
    }
    return pairs;
}

// A sequence, as tablegen wrote it: it takes magnitude(v) bytes from the
// address of a uint32_t, so on a little-endian host the low byte comes first
// and Big5's `A1 40` is spelled 0x40A1.
function seqBytes(v) {
    const n = v >>> 8 === 0 ? 1 : v >>> 16 === 0 ? 2 : v >>> 24 === 0 ? 3 : 4;
    const out = [];
    for (let i = 0; i < n; i++) out.push((v >>> (8 * i)) & 0xff);
    return out;
}

const be32 = (v) => [(v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff];

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1) die("the kernel did not settle after boot");
H.regrid(80, 24, 8, 16);
if (!H.store.files.has("/bin/sh")) die("the archive did not unpack");

H.store.files.set("/bin/iconv", new Uint8Array(readFileSync(opt.binary)));

// The whole table set, because which files a conversion reaches is decided by
// mapper.dir at run time and not by this test.
import { readdirSync, statSync } from "node:fs";
let planted = 0;
(function plant(dir, rel) {
    for (const name of readdirSync(dir)) {
        const from = join(dir, name);
        const at = rel ? rel + "/" + name : name;
        if (statSync(from).isDirectory()) plant(from, at);
        else {
            H.store.files.set("/opt/share/i18n/" + at, new Uint8Array(readFileSync(from)));
            planted++;
        }
    }
})(opt.data, "");

let now = 1;
function run(cmd) {
    if (cmd.length > 60) die(`the command line is ${cmd.length} chars; the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1) die(`the kernel did not settle after: ${cmd}`);
}
const read = (p) => H.store.files.get(p) ?? new Uint8Array();
const text = (p) => new TextDecoder().decode(read(p));

run("export ICONV_PREFIX=/opt");

let checked = 0;
for (const enc of ENCODINGS) {
    current = enc;
    const fwdPath = join(opt.ref, enc);
    const revPath = join(opt.ref, enc + "-rev");
    if (!existsSync(fwdPath)) die(`the corpus has no ${enc}`);

    // Forward: the encoding's bytes to UTF-32BE.
    {
        const pairs = parseRef(fwdPath);
        const input = [], want = [];
        for (const [seq, ucs] of pairs) {
            // A newline after each: without it one entry's bytes would join
            // the next into a multibyte sequence neither of them is. Every
            // encoding here spells it 0x0A and maps it to U+000A.
            input.push(...seqBytes(seq), 0x0a);
            want.push(...be32(ucs), ...be32(0x0a));
        }
        H.store.files.set("/tmp/i", new Uint8Array(input));
        run(`iconv -f ${enc} -t UTF-32BE </tmp/i >/tmp/o 2>/tmp/e`);
        const got = read("/tmp/o");
        const err = text("/tmp/e").trim();
        if (got.length !== want.length) {
            bad(`${enc} forward: ${got.length} bytes out, expected ${want.length}`
                + (err ? `. stderr: ${err}` : ""));
        } else {
            let first = -1;
            for (let i = 0; i < want.length && first < 0; i++)
                if (got[i] !== want[i]) first = (i >> 2) >> 1;
            if (first >= 0)
                bad(`${enc} forward: mapping ${first} (0x${pairs[first][0].toString(16)}) `
                    + `should be U+${pairs[first][1].toString(16).toUpperCase()}`);
            else
                checked += pairs.length;
        }
    }

    // Reverse: UTF-32BE to the encoding's bytes.
    if (existsSync(revPath)) {
        const pairs = parseRef(revPath);
        const input = [], want = [];
        for (const [ucs, seq] of pairs) {
            input.push(...be32(ucs), ...be32(0x0a));
            want.push(...seqBytes(seq), 0x0a);
        }
        H.store.files.set("/tmp/i", new Uint8Array(input));
        run(`iconv -f UTF-32BE -t ${enc} </tmp/i >/tmp/o 2>/tmp/e`);
        const got = read("/tmp/o");
        const err = text("/tmp/e").trim();
        if (got.length !== want.length) {
            bad(`${enc} reverse: ${got.length} bytes out, expected ${want.length}`
                + (err ? `. stderr: ${err}` : ""));
        } else {
            let first = -1;
            for (let i = 0; i < want.length && first < 0; i++)
                if (got[i] !== want[i]) first = i;
            if (first >= 0)
                bad(`${enc} reverse: byte ${first} is 0x${got[first].toString(16)}, `
                    + `expected 0x${want[first].toString(16)}`);
            else
                checked += pairs.length;
        }
    }
}

if (failures.length) {
    for (const f of failures) console.error("convert: " + f);
    console.error(`convert: ${failures.length} directions differ that must not`);
    process.exit(1);
}
console.log(`convert ok: ${checked} mappings over ${EXACT.length} encodings agree with `
            + `GNU libiconv; ${differences.length} of ${INFORMATIONAL.length * 2} known `
            + `citrus-vs-GNU directions differ, as upstream's own suite allows`);
