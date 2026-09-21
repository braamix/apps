import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/xz/xz.wasm");

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
H.store.files.set("/bin/xz", new Uint8Array(readFileSync(BIN)));
H.store.files.set("/bin/unxz", new Uint8Array(readFileSync(BIN)));

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

run("xz -1 -f /tmp/in.txt");
if (!H.store.files.has("/tmp/in.txt.xz"))
    die("missing .xz");
const xz1 = H.store.files.get("/tmp/in.txt.xz");
const py1 = execFileSync("python3", ["-c",
    "import lzma,sys; sys.stdout.buffer.write(lzma.decompress(sys.stdin.buffer.read()))"],
    { input: Buffer.from(xz1) });
if (!Buffer.from(py1).equals(Buffer.from(plain)))
    die("xz -1 body mismatch");

H.store.files.set("/tmp/in.txt", plain);
run("xz -c -1 /tmp/in.txt >/tmp/out.xz");
if (!H.store.files.has("/tmp/in.txt"))
    die("-c removed input");
const xzc = H.store.files.get("/tmp/out.xz");
const pyc = execFileSync("python3", ["-c",
    "import lzma,sys; sys.stdout.buffer.write(lzma.decompress(sys.stdin.buffer.read()))"],
    { input: Buffer.from(xzc) });
if (!Buffer.from(pyc).equals(Buffer.from(plain)))
    die("-c output mismatch");

run("unxz -c /tmp/out.xz >/tmp/out.txt");
const out = H.store.files.get("/tmp/out.txt");
if (!out || dec.decode(out) !== body)
    die("unxz -c mismatch");

H.store.files.set("/tmp/in.txt", plain);
run("xz -1 -F lzma -f /tmp/in.txt");
const lzma1 = H.store.files.get("/tmp/in.txt.lzma");
const pyl = execFileSync("python3", ["-c",
    "import lzma,sys; sys.stdout.buffer.write(lzma.decompress(sys.stdin.buffer.read(), format=lzma.FORMAT_ALONE))"],
    { input: Buffer.from(lzma1) });
if (!Buffer.from(pyl).equals(Buffer.from(plain)))
    die("-F lzma mismatch");

H.store.files.set("/tmp/good.xz", xz1);
run("xz -t /tmp/good.xz");
H.store.files.set("/tmp/bad.xz", xz1.subarray(0, xz1.length - 4));
run("xz -t /tmp/bad.xz");

console.log("roundtrip ok");
