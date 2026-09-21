import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures");
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/gzip/gzip.wasm");
const PLAIN = readFileSync(join(FIX, "hello.txt"));

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
H.store.files.set("/bin/gzip", new Uint8Array(readFileSync(BIN)));

for (const name of ["hello.gz", "hello.bz2", "hello.xz", "hello.zst"]) {
    const path = join(FIX, name);
    if (!existsSync(path)) {
        if (name === "hello.lz")
            continue;
        die(`missing fixture ${name}`);
    }
    H.store.files.set(`/tmp/${name}`, new Uint8Array(readFileSync(path)));
}

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`command too long: ${cmd}`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

const dec = new TextDecoder();
for (const name of ["hello.gz", "hello.bz2", "hello.xz", "hello.zst"]) {
    if (!H.store.files.has(`/tmp/${name}`))
        continue;
    run(`gzip -dc /tmp/${name} >/tmp/out`);
    const out = H.store.files.get("/tmp/out");
    if (!out || !dec.decode(out).includes("hello"))
        die(`decompress failed for ${name}`);
    run(`gzip -t /tmp/${name}`);
}

console.log("formats ok");
