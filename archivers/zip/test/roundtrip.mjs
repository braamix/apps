// One entry in, one entry out, headless.
//
// The decisive check for the port: Braam's own /bin/unzip, written
// independently against Package_Formats.md §5.2, reads what this zip writes.
// Stored and deflated both, and the deflated one for a body big enough to be
// worth compressing — a short one deflates larger than it started.

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
        console.error("usage: roundtrip.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`roundtrip: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

// After the paths are checked: the harness exits the process itself, and would
// not say what to build.
const H = await import(join(CORE, "test/system/harness.mjs"));

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
if (!H.store.files.has("/bin/sh"))
    die("the archive did not unpack");

// Planted, not packed: exec takes any stamped path, and /bin is ordinary store
// content once boot has unpacked the archive.
H.store.files.set("/bin/zip", new Uint8Array(readFileSync(opt.binary)));

const enc = new TextEncoder();
const dec = new TextDecoder();

// Compressible, and long enough that deflate beats store.
let body = "";
for (let i = 0; i < 400; i++)
    body += `line ${i}: the quick brown fox jumps over the lazy dog\n`;
const WANT = enc.encode(body);

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`the command line is ${cmd.length} keys, and the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`\`${cmd}\` left the kernel with work to do`);
};

const same = (a, b) => {
    if (!a || !b || a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; i++)
        if (a[i] !== b[i])
            return false;
    return true;
};

H.store.files.set("/tmp/f", WANT);

// ex2in strips the leading slash, so the entry is "tmp/f".
const sizes = {};
for (const [flag, tag] of [["-0", "stored"], ["-9", "deflated"]]) {
    const zip = `/tmp/${tag}.zip`;
    run(`zip ${flag} -q ${zip} /tmp/f`);

    const made = H.store.files.get(zip);
    if (!made || !made.length)
        die(`zip ${flag} wrote no archive`);
    sizes[tag] = made.length;

    // The signature, before anything else is believed.
    if (!(made[0] === 0x50 && made[1] === 0x4b && made[2] === 3 && made[3] === 4))
        die(`${tag}: the archive does not start with a local header signature`);

    run(`unzip -p ${zip} tmp/f > /tmp/${tag}.out`);
    const back = H.store.files.get(`/tmp/${tag}.out`);
    if (!same(back, WANT))
        die(`${tag}: unzip gave back ${back ? back.length : 0} bytes of ${WANT.length}`);
}

if (sizes.deflated >= sizes.stored)
    die(`-9 made ${sizes.deflated} bytes and -0 made ${sizes.stored}: deflate did nothing`);

// The listing names the entry and its uncompressed size.
run("unzip -l /tmp/deflated.zip > /tmp/list");
const list = dec.decode(H.store.files.get("/tmp/list") || new Uint8Array(0));
if (!new RegExp(`^\\s*${WANT.length}\\s+tmp/f$`, "m").test(list))
    die(`the listing does not name the entry: ${JSON.stringify(list)}`);

// Two runs of the same input give the same bytes. The harness clock is frozen,
// so the DOS stamp cannot move; anything else that differs is a bug.
run("zip -9 -q /tmp/again.zip /tmp/f");
if (!same(H.store.files.get("/tmp/again.zip"), H.store.files.get("/tmp/deflated.zip")))
    die("two runs over the same file wrote different archives");

console.log(`roundtrip ok: ${WANT.length} bytes -> ${sizes.deflated} deflated, ` +
            `${sizes.stored} stored, and /bin/unzip reads both`);
