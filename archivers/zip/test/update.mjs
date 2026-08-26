// An archive that already exists, headless.
//
// The update path: readzipfile() reads what is there, zipcopy() moves an
// entry that has not changed straight across without recompressing it, and
// -u, -f, -d and -m decide which entries survive. Every result is read back
// with Braam's own /bin/unzip.

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
        console.error("usage: update.mjs [--kernel=<wasm>] [--rootfs=<zip>] [--binary=<wasm>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`update: ${msg}`);
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

const dec = new TextDecoder();

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`the command line is ${cmd.length} keys, and the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`\`${cmd}\` left the kernel with work to do`);
};

// The entries an archive holds, by name, in the order unzip lists them.
const names = (zip) => {
    run(`unzip -l ${zip} > /tmp/l 2>&1`);
    const out = dec.decode(H.store.files.get("/tmp/l") || new Uint8Array(0));
    if (/not found|cannot|error/i.test(out))
        die(`unzip would not read ${zip}: ${JSON.stringify(out)}`);
    return out.trim().split("\n").map((l) => l.trim().split(/\s+/)[1]).filter(Boolean);
};

const content = (zip, entry) => {
    run(`unzip -p ${zip} ${entry} > /tmp/c 2>&1`);
    return dec.decode(H.store.files.get("/tmp/c") || new Uint8Array(0));
};

const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

run("echo alpha > /home/a");
run("echo beta > /home/b");
run("echo gamma > /home/c");

// 1. A new archive, then a second entry added to it. The second run has to
//    read the first entry back and copy it across, which is zipcopy's job.
run("zip -q /home/x.zip /home/a");
if (!same(names("/home/x.zip"), ["home/a"]))
    die(`the new archive holds ${JSON.stringify(names("/home/x.zip"))}`);

run("zip -q /home/x.zip /home/b");
if (!same(names("/home/x.zip"), ["home/a", "home/b"]))
    die(`after adding, it holds ${JSON.stringify(names("/home/x.zip"))}`);
if (content("/home/x.zip", "home/a") !== "alpha\n")
    die("the entry that was copied across did not survive");

// 2. -u takes what changed and adds what is new, and leaves the rest alone.
run("echo BETA-CHANGED > /home/b");
run("zip -q -u /home/x.zip /home/b /home/c");
if (!same(names("/home/x.zip"), ["home/a", "home/b", "home/c"]))
    die(`after -u it holds ${JSON.stringify(names("/home/x.zip"))}`);
if (content("/home/x.zip", "home/b") !== "BETA-CHANGED\n")
    die(`-u did not replace the entry: ${JSON.stringify(content("/home/x.zip", "home/b"))}`);
if (content("/home/x.zip", "home/a") !== "alpha\n")
    die("-u disturbed an entry it was not asked about");

// 3. -f freshens what is in the archive and adds nothing.
run("echo ALPHA-2 > /home/a");
run("echo delta > /home/d");
run("zip -q -f /home/x.zip /home/a /home/d");
if (!same(names("/home/x.zip"), ["home/a", "home/b", "home/c"]))
    die(`-f added an entry it should not have: ${JSON.stringify(names("/home/x.zip"))}`);
if (content("/home/x.zip", "home/a") !== "ALPHA-2\n")
    die("-f did not freshen the entry");

// 4. -d removes one, and the rest come through untouched.
run("zip -q -d /home/x.zip home/b");
if (!same(names("/home/x.zip"), ["home/a", "home/c"]))
    die(`after -d it holds ${JSON.stringify(names("/home/x.zip"))}`);
if (content("/home/x.zip", "home/c") !== "gamma\n")
    die("-d disturbed an entry it was not asked about");

// 5. -m moves: the file goes in and is then deleted.
run("zip -q -m /home/x.zip /home/d");
if (!same(names("/home/x.zip"), ["home/a", "home/c", "home/d"]))
    die(`after -m it holds ${JSON.stringify(names("/home/x.zip"))}`);
if (H.store.files.has("/home/d"))
    die("-m left the file behind");

console.log(`update ok: added, updated, freshened, deleted and moved — ` +
            `${names("/home/x.zip").length} entries left`);
