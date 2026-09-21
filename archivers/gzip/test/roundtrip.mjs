import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { gzipSync, gunzipSync } from "node:zlib";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/gzip/gzip.wasm");

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
H.store.files.set("/bin/gzip", new Uint8Array(readFileSync(BIN)));

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
H.store.files.set("/tmp/in.txt", plain);

run("gzip -1 -f /tmp/in.txt");
if (!H.store.files.has("/tmp/in.txt.gz"))
    die("missing .gz");
const gz1 = H.store.files.get("/tmp/in.txt.gz");
if (!gunzipSync(gz1).equals(plain))
    die("gzip -1 body mismatch");

H.store.files.set("/tmp/in.txt", plain);
run("gzip -9 -f /tmp/in.txt");
const gz9 = H.store.files.get("/tmp/in.txt.gz");
if (!gunzipSync(gz9).equals(plain))
    die("gzip -9 body mismatch");

H.store.files.set("/tmp/in.txt", plain);
run("gzip -c -1 /tmp/in.txt >/tmp/out.gz");
if (!H.store.files.has("/tmp/in.txt"))
    die("-c removed input");
const gzc = H.store.files.get("/tmp/out.gz");
if (!gunzipSync(gzc).equals(plain))
    die("-c output mismatch");

const nodeGz = gzipSync(plain);
H.store.files.set("/tmp/good.gz", nodeGz);
run("gzip -t /tmp/good.gz");
H.store.files.set("/tmp/bad.gz", nodeGz.subarray(0, nodeGz.length - 4));
run("gzip -t /tmp/bad.gz");

console.log("roundtrip ok");
