import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { zstdCompressSync, zstdDecompressSync } from "node:zlib";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/zstd/zstd.wasm");

const die = (msg) => {
    console.error(`roundtrip: ${msg}`);
    process.exit(1);
};

if (!existsSync(BIN))
    die("run make first");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
const bin = new Uint8Array(readFileSync(BIN));
H.store.files.set("/bin/zstd", bin);
H.store.files.set("/bin/unzstd", bin);

const enc = new TextEncoder();
const dec = new TextDecoder();
let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`command too long: ${cmd}`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

let body = "";
for (let i = 0; i < 200; i++)
    body += `line ${i}: compress me\n`;
const plain = enc.encode(body);

/* -1, the cheapest level, and -19, which is the largest window a process can
 * afford: 20 and up exceed the 100 MB cap. */
for (const level of ["-1", "-19"]) {
    H.store.files.set("/tmp/in.txt", plain);
    run(`zstd ${level} -f /tmp/in.txt`);
    if (!H.store.files.has("/tmp/in.txt.zst"))
        die(`${level}: missing .zst`);
    const zst = H.store.files.get("/tmp/in.txt.zst");
    if (!zstdDecompressSync(zst).equals(Buffer.from(plain)))
        die(`${level}: body mismatch`);
    if (!H.store.files.has("/tmp/in.txt"))
        die(`${level}: input removed without --rm`);
}

/* -c writes the frame to stdout and keeps the source. */
H.store.files.set("/tmp/in.txt", plain);
run("zstd -c -1 /tmp/in.txt >/tmp/out.zst");
if (!zstdDecompressSync(H.store.files.get("/tmp/out.zst")).equals(Buffer.from(plain)))
    die("-c output mismatch");

/* Node's frame, decompressed by this binary, and then its own. */
H.store.files.set("/tmp/node.zst", new Uint8Array(zstdCompressSync(Buffer.from(plain))));
run("unzstd -c /tmp/node.zst >/tmp/node.txt");
if (dec.decode(H.store.files.get("/tmp/node.txt")) !== body)
    die("unzstd of node's frame mismatch");

run("zstd -d -c /tmp/out.zst >/tmp/out.txt");
if (dec.decode(H.store.files.get("/tmp/out.txt")) !== body)
    die("-d -c mismatch");

/* --rm takes the source; -o names the destination. */
H.store.files.set("/tmp/rm.txt", plain);
run("zstd --rm -1 /tmp/rm.txt -o /tmp/rm.zst");
if (H.store.files.has("/tmp/rm.txt"))
    die("--rm kept the input");
if (!zstdDecompressSync(H.store.files.get("/tmp/rm.zst")).equals(Buffer.from(plain)))
    die("-o output mismatch");

/* An empty file is a valid frame. */
H.store.files.set("/tmp/e", new Uint8Array(0));
run("zstd -1 -f /tmp/e");
if (zstdDecompressSync(H.store.files.get("/tmp/e.zst")).length !== 0)
    die("empty frame is not empty");

/* Through the pipe, with no name to take a suffix from. */
run("zstd -1 </tmp/in.txt >/tmp/pipe.zst");
if (!zstdDecompressSync(H.store.files.get("/tmp/pipe.zst")).equals(Buffer.from(plain)))
    die("stdin/stdout mismatch");

/* -t accepts the good frame and refuses a truncated one, and neither writes. */
const good = H.store.files.get("/tmp/out.zst");
H.store.files.set("/tmp/good.zst", good);
run("zstd -t /tmp/good.zst");
H.store.files.set("/tmp/bad.zst", good.subarray(0, good.length - 4));
run("zstd -t /tmp/bad.zst");
if (H.store.files.has("/tmp/bad"))
    die("-t wrote a file");

console.log("roundtrip ok: levels 1 and 19, -c, -o, --rm, the pipe, an empty frame and -t");
