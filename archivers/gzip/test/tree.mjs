import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const BIN = join(resolve(dirname(fileURLToPath(import.meta.url)), "../../.."),
                 "build/archivers/gzip/gzip.wasm");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
H.store.files.set("/bin/gzip", new Uint8Array(readFileSync(BIN)));

let now = 100;
H.submit("gzip -V", now++);
H.run(now++);
H.submit("gzip -h", now++);
H.run(now++);

console.log("tree ok");
