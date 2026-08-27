// Does it convert at all: UTF-8 to KOI8-R and back, and `iconv -l`.
//
// The data is planted under /opt rather than installed, and ICONV_PREFIX
// points at it — the same escape hatch a program run from a plain path uses.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
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
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

// Checked before the harness is imported, because it exits the process itself
// and would not say what to build.
for (const [what, path, how] of [
    ["kernel", opt.kernel, "make -C ../braam-core"],
    ["rootfs", opt.rootfs, "make -C ../braam-core"],
    ["iconv", opt.binary, "make"],
    ["the i18n tree", opt.data, "make"],
]) {
    if (!existsSync(path)) {
        console.error(`smoke: no ${what} at ${path} — run \`${how}\``);
        process.exit(1);
    }
}

const H = await import(join(CORE, "test/system/harness.mjs"));

function die(msg) {
    console.error("smoke: " + msg);
    process.exit(1);
}

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1) die("the kernel did not settle after boot");
H.regrid(80, 24, "resize returned no screen descriptor");
if (!H.store.files.has("/bin/sh")) die("the archive did not unpack");

H.store.files.set("/bin/iconv", new Uint8Array(readFileSync(opt.binary)));

// Only what these two conversions touch: the four index files, and the KOI8-R
// pair with the two mappers it is built from.
let planted = 0;
function plant(rel) {
    const from = join(opt.data, rel);
    if (!existsSync(from)) die(`the generated tree has no ${rel}`);
    H.store.files.set("/opt/share/i18n/" + rel, new Uint8Array(readFileSync(from)));
    planted++;
}
for (const f of ["esdb/esdb.dir", "esdb/esdb.dir.db", "esdb/esdb.alias", "esdb/esdb.alias.db",
                 "csmapper/mapper.dir", "csmapper/charset.pivot", "csmapper/charset.pivot.pvdb",
                 "esdb/KOI/KOI8-R.esdb", "esdb/UTF/UTF-8.esdb",
                 "csmapper/KOI/KOI8-R%UCS.mps", "csmapper/KOI/UCS%KOI8-R.mps",
                 "csmapper/KOI/GOST19768-74%UCS.mps", "csmapper/KOI/UCS%GOST19768-74.mps"])
    plant(f);

let now = 1;
function run(cmd) {
    if (cmd.length > 60) die(`the command line is ${cmd.length} chars; the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1) die(`the kernel did not settle after: ${cmd}`);
}
const read = (p) => H.store.files.get(p);
const text = (p) => new TextDecoder().decode(read(p) ?? new Uint8Array());

// "Привет, мир" — Cyrillic, which is what KOI8-R is for.
const SRC = "Привет, мир!\n";
H.store.files.set("/tmp/u", new TextEncoder().encode(SRC));

run("export ICONV_PREFIX=/opt");
run("iconv -f UTF-8 -t KOI8-R </tmp/u >/tmp/k 2>/tmp/e1");

const err1 = text("/tmp/e1");
const koi = read("/tmp/k");
if (!koi || koi.length === 0)
    die(`nothing came out of the forward conversion. stderr: ${err1.trim() || "(empty)"}`);

// KOI8-R is one byte per character, so the length is the codepoint count.
const want = [...SRC].length;
if (koi.length !== want)
    die(`forward conversion gave ${koi.length} bytes, expected ${want}`);
// П is 0xF0 in KOI8-R.
if (koi[0] !== 0xf0)
    die(`forward conversion starts 0x${koi[0].toString(16)}, expected 0xf0`);

run("iconv -f KOI8-R -t UTF-8 </tmp/k >/tmp/u2 2>/tmp/e2");
const back = text("/tmp/u2");
if (back !== SRC)
    die(`round trip differs:\n  got  ${JSON.stringify(back)}\n  want ${JSON.stringify(SRC)}`);

run("iconv -l >/tmp/l 2>/tmp/e3");
const list = text("/tmp/l");
for (const name of ["KOI8-R", "UTF-8"])
    if (!list.includes(name))
        die(`iconv -l does not name ${name}. stderr: ${text("/tmp/e3").trim()}`);

console.log(`smoke ok: ${planted} files planted, round trip exact, `
            + `-l names ${list.split(/\s+/).filter(Boolean).length} encodings`);
