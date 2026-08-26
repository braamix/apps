// ^C part way through, headless.
//
// zip parks on the read of the file it is compressing, so that is where a
// signal reaches it. The two command lines are queued before the first tick:
// the shell takes its turn where zip parks, which is the moment under test.
//
// What must hold afterwards is the reason zip writes a temporary archive at
// all: the name it was asked for is untouched, and the temporary is gone.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/archivers/zip/zip.wasm"),
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: interrupt.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`interrupt: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

const H = await import(join(CORE, "test/system/harness.mjs"));

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/zip", new Uint8Array(readFileSync(opt.binary)));

const enc = new TextEncoder();
const dec = new TextDecoder();

// Big enough that the compression is still going when the shell runs the
// kill: one read is 64 KB, so this is a good many parks.
let body = "";
for (let i = 0; i < 20000; i++)
    body += `line ${i}: the quick brown fox jumps over the lazy dog\n`;
H.store.files.set("/home/big", enc.encode(body));

// Queued before the first tick: the shell gets its turn where zip parks.
for (const line of ["zip -9 /home/a.zip /home/big &", "kill -INT %1"]) {
    if (line.length > 60)
        die(`the command line is ${line.length} keys, and the ring holds 64`);
    H.type(line);
    H.press(H.KEY.ENTER);
}
if (H.run(100) !== -1)
    die("the kernel still had work to do");

const s = H.screen();
const flat = H.rows(s).join(" ").replace(/\s+/g, " ");

// No archive: the temporary is written first and only renamed over the name
// at the end, so an interrupted run leaves nothing at all.
if (H.store.files.has("/home/a.zip"))
    die(`an interrupted run left an archive of ` +
        `${H.store.files.get("/home/a.zip").length} bytes`);

// And nothing is left behind beside it.
const litter = [...H.store.files.keys()].filter((p) => /\/zi[0-9a-f]{8}$/.test(p));
if (litter.length)
    die(`the temporary archive was left behind: ${JSON.stringify(litter)}`);

// The shell is still there and still works.
const alive = H.submit("echo alive", 101);
if (!H.rows(alive).includes("alive"))
    die(`the shell did not come back: ${JSON.stringify(H.rows(alive).filter((l) => l.trim()))}`);

console.log("interrupt ok: no archive, no temporary, and the shell came back");
