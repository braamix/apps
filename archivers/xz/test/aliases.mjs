import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures");
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/xz/xz.wasm");

const die = (msg) => {
    console.error(`aliases: ${msg}`);
    process.exit(1);
};

const wasm = readFileSync(BIN);
const xz = readFileSync(join(FIX, "hello.xz"));
const lzma = readFileSync(join(FIX, "hello.lzma"));

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
H.store.files.set("/bin/unxz", new Uint8Array(wasm));
H.store.files.set("/bin/xzcat", new Uint8Array(wasm));
H.store.files.set("/bin/lzcat", new Uint8Array(wasm));
H.store.files.set("/tmp/x.xz", new Uint8Array(xz));
H.store.files.set("/tmp/x.lzma", new Uint8Array(lzma));

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`command too long: ${cmd}`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

run("unxz -c /tmp/x.xz >/tmp/a");
run("xzcat /tmp/x.xz >/tmp/b");
run("lzcat /tmp/x.lzma >/tmp/c");
const dec = new TextDecoder();
const a = H.store.files.get("/tmp/a");
const b = H.store.files.get("/tmp/b");
const c = H.store.files.get("/tmp/c");
const ta = dec.decode(a);
if (!a || !b || !c || ta !== dec.decode(b) || ta !== dec.decode(c))
    die("alias output mismatch");

console.log("aliases ok");
