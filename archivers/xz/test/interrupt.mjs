import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const BIN = join(resolve(dirname(fileURLToPath(import.meta.url)), "../../.."),
                 "build/archivers/xz/xz.wasm");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
H.store.files.set("/bin/xz", new Uint8Array(readFileSync(BIN)));

let body = "";
for (let i = 0; i < 8000; i++)
    body += `line ${i}\n`;
H.store.files.set("/tmp/big.txt", new TextEncoder().encode(body));

H.submit("xz -1 -f /tmp/big.txt &", 100);
H.submit("kill -INT %1", 101);
for (let now = 102, i = 0; i < 30 && H.run(now++) !== -1; i++) { /* drain */ }

console.log("interrupt ok");
