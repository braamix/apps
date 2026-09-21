// The four names the package links to this binary. argv[0] is what decides
// which of them the program behaves as, so this is the one thing a package that
// ships links has to assert.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures");
const APPS = resolve(HERE, "../../..");
const BIN = join(APPS, "build/archivers/zstd/zstd.wasm");

const die = (msg) => {
    console.error(`aliases: ${msg}`);
    process.exit(1);
};

if (!existsSync(BIN))
    die("run make first");

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
H.run(0);
H.regrid(80, 24);
const wasm = new Uint8Array(readFileSync(BIN));
for (const name of ["zstd", "unzstd", "zstdcat", "zstdmt"])
    H.store.files.set(`/bin/${name}`, wasm);

const PLAIN = readFileSync(join(FIX, "hello.txt"));
H.store.files.set("/tmp/x.zst", new Uint8Array(readFileSync(join(FIX, "hello.zst"))));
H.store.files.set("/tmp/hello.txt", new Uint8Array(PLAIN));

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`command too long: ${cmd}`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`still running: ${cmd}`);
};

/* unzstd decompresses without -d; zstdcat writes to stdout without -c. */
run("unzstd -q -c /tmp/x.zst >/tmp/a");
run("zstdcat /tmp/x.zst >/tmp/b");
for (const p of ["/tmp/a", "/tmp/b"]) {
    const got = H.store.files.get(p);
    if (!got || !Buffer.from(got).equals(PLAIN))
        die(`${p}: mismatch`);
}

/* zstdcat is a cat: it passes a file that is not compressed through. */
run("zstdcat /tmp/hello.txt >/tmp/c");
if (!Buffer.from(H.store.files.get("/tmp/c")).equals(PLAIN))
    die("zstdcat did not pass through");

/* zstdmt compresses; libzstd here has one thread, so it is zstd with a note. */
run("zstdmt -q -1 /tmp/hello.txt -o /tmp/mt.zst");
run("unzstd -q -c /tmp/mt.zst >/tmp/d");
if (!Buffer.from(H.store.files.get("/tmp/d")).equals(PLAIN))
    die("zstdmt roundtrip mismatch");

console.log("aliases ok: unzstd, zstdcat and zstdmt, and zstdcat passes plain text through");
