import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures");
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/gzip/gzip.wasm");

const die = (msg) => {
    console.error(`aliases: ${msg}`);
    process.exit(1);
};

const wasm = readFileSync(BIN);
const gz = readFileSync(join(FIX, "hello.gz"));

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
H.store.files.set("/bin/gunzip", new Uint8Array(wasm));
H.store.files.set("/bin/zcat", new Uint8Array(wasm));
H.store.files.set("/tmp/x.gz", new Uint8Array(gz));

let now = 100;
const run = (cmd) => {
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

run("gunzip -c /tmp/x.gz >/tmp/a");
run("zcat /tmp/x.gz >/tmp/b");
const dec = new TextDecoder();
const a = H.store.files.get("/tmp/a");
const b = H.store.files.get("/tmp/b");
if (!a || !b || dec.decode(a) !== dec.decode(b))
    die("alias output mismatch");

console.log("aliases ok");
