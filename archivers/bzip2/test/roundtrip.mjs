import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/bzip2/bzip2.wasm");

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
H.store.files.set("/bin/bzip2", new Uint8Array(readFileSync(BIN)));
H.store.files.set("/bin/bunzip2", new Uint8Array(readFileSync(BIN)));

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

run("bzip2 -1 -f /tmp/in.txt");
if (!H.store.files.has("/tmp/in.txt.bz2"))
    die("missing .bz2");
const bz1 = H.store.files.get("/tmp/in.txt.bz2");
const py1 = execFileSync("python3", ["-c",
    "import bz2,sys; sys.stdout.buffer.write(bz2.decompress(sys.stdin.buffer.read()))"],
    { input: Buffer.from(bz1) });
if (!Buffer.from(py1).equals(Buffer.from(plain)))
    die("bzip2 -1 body mismatch");

H.store.files.set("/tmp/in.txt", plain);
run("bzip2 -9 -f /tmp/in.txt");
const bz9 = H.store.files.get("/tmp/in.txt.bz2");
const py9 = execFileSync("python3", ["-c",
    "import bz2,sys; sys.stdout.buffer.write(bz2.decompress(sys.stdin.buffer.read()))"],
    { input: Buffer.from(bz9) });
if (!Buffer.from(py9).equals(Buffer.from(plain)))
    die("bzip2 -9 body mismatch");

H.store.files.set("/tmp/in.txt", plain);
run("bzip2 -c -1 /tmp/in.txt >/tmp/out.bz2");
if (!H.store.files.has("/tmp/in.txt"))
    die("-c removed input");
const bzc = H.store.files.get("/tmp/out.bz2");
const pyc = execFileSync("python3", ["-c",
    "import bz2,sys; sys.stdout.buffer.write(bz2.decompress(sys.stdin.buffer.read()))"],
    { input: Buffer.from(bzc) });
if (!Buffer.from(pyc).equals(Buffer.from(plain)))
    die("-c output mismatch");

run("bunzip2 -c /tmp/out.bz2 >/tmp/out.txt");
const out = H.store.files.get("/tmp/out.txt");
if (!out || dec.decode(out) !== body)
    die("bunzip2 -c mismatch");

H.store.files.set("/tmp/good.bz2", bz1);
run("bzip2 -t /tmp/good.bz2");
H.store.files.set("/tmp/bad.bz2", bz1.subarray(0, bz1.length - 4));
run("bzip2 -t /tmp/bad.bz2");

console.log("roundtrip ok");
