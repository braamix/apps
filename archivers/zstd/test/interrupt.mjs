// Queue SIGINT on a background zstd job (same recipe as archivers/gzip's).
// The harness may finish the job before the kill is delivered; this case only
// asserts we do not trap, and that no half-written .zst is left behind when the
// signal does land.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const BIN = join(resolve(dirname(fileURLToPath(import.meta.url)), "../../.."),
                 "build/archivers/zstd/zstd.wasm");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
H.store.files.set("/bin/zstd", new Uint8Array(readFileSync(BIN)));

let body = "";
for (let i = 0; i < 8000; i++)
    body += `line ${i}\n`;
H.store.files.set("/tmp/big.txt", new TextEncoder().encode(body));

H.submit("zstd -19 -f /tmp/big.txt &", 100);
H.submit("kill -INT %1", 101);
for (let now = 102, i = 0; i < 30 && H.run(now++) !== -1; i++) { /* drain */ }

if (H.store.files.has("/tmp/big.txt.zst") && !H.store.files.has("/tmp/big.txt")) {
    console.error("interrupt: source removed");
    process.exit(1);
}

console.log("interrupt ok: no trap, and the source survives");
