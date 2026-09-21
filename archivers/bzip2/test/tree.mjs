import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const BIN = join(resolve(dirname(fileURLToPath(import.meta.url)), "../../.."),
                 "build/archivers/bzip2/bzip2.wasm");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
H.store.files.set("/bin/bzip2", new Uint8Array(readFileSync(BIN)));

let now = 100;
H.submit("bzip2 -V", now++);
H.run(now++);
H.submit("bzip2 -h", now++);
H.run(now++);

console.log("tree ok");
