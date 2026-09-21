// Fidelity: the frames this binary writes, byte for byte against the frames
// upstream's own zstd 1.6.0 wrote for the same input and the same options.
// The compressor is libzstd itself, so an exact match is the right bar, and a
// mismatch says the CLI passed a different parameter rather than that the
// encoder drifted.
//
// fixtures/mkframes.sh regenerates the goldens and says how upstream was built.
// --format=gzip is not among them: zlib's header carries an OS byte that is the
// build's, not the input's, so that pair is only ever equal below the header.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { gunzipSync } from "node:zlib";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures");
const BIN = join(resolve(HERE, "../../.."), "build/archivers/zstd/zstd.wasm");

const die = (msg) => {
    console.error(`frames: ${msg}`);
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

const PLAIN = readFileSync(join(FIX, "frames.txt"));
H.store.files.set("/tmp/in", new Uint8Array(PLAIN));

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`command too long: ${cmd}`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

const cases = [
    ["-1", "frames.1.zst"],
    ["-3", "frames.3.zst"],
    ["-19", "frames.19.zst"],
    ["--fast=3", "frames.fast3.zst"],
    ["--long=20 -6", "frames.long20.zst"],
    ["-6 --no-check", "frames.nocheck.zst"],
    ["-5 --format=xz", "frames.5.xz"],
    ["-5 --format=lzma", "frames.5.lzma"],
];

for (const [args, golden] of cases) {
    run(`zstd -q ${args} -c /tmp/in >/tmp/o`);
    const got = Buffer.from(H.store.files.get("/tmp/o") ?? new Uint8Array(0));
    const want = readFileSync(join(FIX, golden));
    if (!got.equals(want))
        die(`${args}: ${got.length} bytes, upstream's ${want.length}` +
            (got.length === want.length ? " -- same length, different bytes" : ""));
}

// gzip below the ten-byte header, which is where zlib's OS_CODE sits.
run("zstd -q -5 --format=gzip -c /tmp/in >/tmp/o");
const gz = Buffer.from(H.store.files.get("/tmp/o"));
if (!gunzipSync(gz).equals(PLAIN))
    die("--format=gzip does not round trip");

console.log(`frames ok: ${cases.length} encodings identical to upstream zstd 1.6.0`);
