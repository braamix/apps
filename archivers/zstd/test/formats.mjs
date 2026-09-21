// The three formats this build reads and writes, each checked both ways: a
// fixture made by someone else's encoder is decompressed, and what this binary
// writes is decompressed by node or python.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { gunzipSync, zstdDecompressSync } from "node:zlib";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures");
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/zstd/zstd.wasm");

const die = (msg) => {
    console.error(`formats: ${msg}`);
    process.exit(1);
};

if (!existsSync(BIN))
    die("run make first");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
H.store.files.set("/bin/zstd", new Uint8Array(readFileSync(BIN)));

const PLAIN = readFileSync(join(FIX, "hello.txt"));
for (const name of ["hello.txt", "hello.zst", "hello.gz", "hello.xz", "hello.lzma"])
    H.store.files.set(`/tmp/${name}`, new Uint8Array(readFileSync(join(FIX, name))));

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`command too long: ${cmd}`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

const unxz = (buf, alone) => execFileSync("python3", ["-c",
    `import lzma,sys; sys.stdout.buffer.write(lzma.decompress(sys.stdin.buffer.read()` +
    (alone ? ", format=lzma.FORMAT_ALONE))" : "))")],
    { input: Buffer.from(buf) });

/* Reading: four frames from four other encoders, all through -d, which sniffs
 * the magic rather than the suffix. */
for (const name of ["hello.zst", "hello.gz", "hello.xz", "hello.lzma"]) {
    run(`zstd -q -d -c /tmp/${name} >/tmp/out`);
    const out = H.store.files.get("/tmp/out");
    if (!out || !Buffer.from(out).equals(PLAIN))
        die(`-d mismatch for ${name}`);
    run(`zstd -q -t /tmp/${name}`);
}

/* Writing: each --format, checked against a decoder that is not this one. */
const cases = [
    ["zstd", "zst", (b) => zstdDecompressSync(Buffer.from(b))],
    ["gzip", "gz", (b) => gunzipSync(Buffer.from(b))],
    ["xz", "xz", (b) => unxz(b, false)],
    ["lzma", "lzma", (b) => unxz(b, true)],
];
for (const [format, ext, decode] of cases) {
    run(`zstd -q --format=${format} -f /tmp/hello.txt`);
    const path = `/tmp/hello.txt.${ext}`;
    if (!H.store.files.has(path))
        die(`--format=${format}: missing ${path}`);
    if (!Buffer.from(decode(H.store.files.get(path))).equals(PLAIN))
        die(`--format=${format}: body mismatch`);
}

/* lz4 is the one format of upstream's four that this build has not got, so its
 * magic must be named rather than reported as an unknown header. */
const lz4 = Buffer.concat([Buffer.from([0x04, 0x22, 0x4d, 0x18]), Buffer.alloc(32)]);
H.store.files.set("/tmp/hello.lz4", new Uint8Array(lz4));
run("zstd -d -c /tmp/hello.lz4 >/tmp/lz4.out");
if (!H.output(H.screen()).join("").includes("lz4 file cannot be uncompressed"))
    die("lz4 frame not named");

console.log("formats ok: zstd, gzip, xz and lzma read and written; lz4 named, not guessed");
