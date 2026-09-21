// The directory half: -r walks, --output-dir-flat and --output-dir-mirror
// decide where the output lands, and -l reads a listing back. These are the
// paths that reach b_opendir and b_readdir, which nothing else here does.
//
// Each phase gets a tree of its own, because -r takes whatever is in the
// directory -- including the .zst an earlier phase left there.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { zstdDecompressSync } from "node:zlib";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const BIN = join(resolve(dirname(fileURLToPath(import.meta.url)), "../../.."),
                 "build/archivers/zstd/zstd.wasm");

const die = (msg) => {
    console.error(`tree: ${msg}`);
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

const enc = new TextEncoder();
let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`command too long: ${cmd}`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

const A = "the first file\n".repeat(50);
const B = "the second file\n".repeat(50);
const plant = (root) => {
    run(`mkdir -p ${root}/s`);
    H.store.files.set(`${root}/a.txt`, enc.encode(A));
    H.store.files.set(`${root}/s/b.txt`, enc.encode(B));
};

/* -r walks the tree and writes each .zst beside its source. */
plant("/tmp/d");
run("zstd -q -r -1 /tmp/d");
for (const [path, text] of [["/tmp/d/a.txt", A], ["/tmp/d/s/b.txt", B]]) {
    const zst = H.store.files.get(`${path}.zst`);
    if (!zst)
        die(`missing ${path}.zst`);
    if (zstdDecompressSync(zst).toString() !== text)
        die(`${path}.zst body mismatch`);
}

/* --output-dir-flat collects them into one directory, losing the subdirectory.
 * The directory has to exist first -- upstream does not create this one. */
plant("/tmp/e");
run("mkdir /tmp/f");
run("zstd -q -r -1 /tmp/e --output-dir-flat /tmp/f");
if (!H.store.files.has("/tmp/f/a.txt.zst") || !H.store.files.has("/tmp/f/b.txt.zst"))
    die("--output-dir-flat did not flatten");

/* --output-dir-mirror keeps the subdirectory, and does create what it needs. */
plant("/tmp/g");
run("zstd -q -r -1 /tmp/g --output-dir-mirror /tmp/m");
if (!H.store.files.has("/tmp/m/tmp/g/s/b.txt.zst"))
    die("--output-dir-mirror did not mirror");

/* -l over several frames. */
run("zstd -l /tmp/d/a.txt.zst /tmp/d/s/b.txt.zst");
const listing = H.output(H.screen()).join("\n");
if (!listing.includes("a.txt.zst") || !listing.includes("b.txt.zst"))
    die("-l did not list both");

/* -d -r walks the flat directory back, and --rm takes the frames with it. */
run("zstd -q -d -r --rm /tmp/f");
if (H.store.files.has("/tmp/f/a.txt.zst"))
    die("-d --rm kept the frame");
if (H.store.files.get("/tmp/f/a.txt").length !== A.length)
    die("-d -r output wrong size");

console.log("tree ok: -r walks, --output-dir-flat and --output-dir-mirror place, -l lists");
