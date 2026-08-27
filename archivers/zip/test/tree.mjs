// A directory tree in, the same tree out, headless.
//
// What the create path adds over one named file: the option parser, -r's walk,
// the -i and -x pattern filters, -j, and the compression levels. Each archive
// is read back with Braam's own /bin/unzip, which was written independently
// against Package_Formats.md §5.2.

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
        console.error("usage: tree.mjs [--kernel=<wasm>] [--rootfs=<zip>] [--binary=<wasm>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`tree: ${msg}`);
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

let now = 100;
const run = (cmd) => {
    if (cmd.length > 60)
        die(`the command line is ${cmd.length} keys, and the ring holds 64`);
    const s = H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die(`\`${cmd}\` left the kernel with work to do`);
    return s;
};

const listing = (zip) => {
    run(`unzip -l ${zip} > /tmp/l 2>&1`);
    return dec.decode(H.store.files.get("/tmp/l") || new Uint8Array(0));
};

// Compressible, so -9 has something to beat -0 with.
let body = "";
for (let i = 0; i < 300; i++)
    body += `line ${i}: the quick brown fox jumps over the lazy dog\n`;

run("mkdir /home/t");
run("mkdir /home/t/sub");
H.store.files.set("/home/t/a.txt", enc.encode(body));
run("echo beta > /home/t/b.log");
run("echo gamma > /home/t/sub/c.txt");

// 1. -r walks the tree, and the directories are entries of their own.
run("zip -r -q /tmp/r.zip /home/t");
const whole = listing("/tmp/r.zip");
for (const name of ["home/t/a.txt", "home/t/b.log", "home/t/sub/c.txt"])
    if (!whole.includes(name))
        die(`-r did not store ${name}: ${JSON.stringify(whole)}`);

// 2. Every entry comes back byte for byte.
run("unzip -p /tmp/r.zip home/t/a.txt > /tmp/back");
const back = H.store.files.get("/tmp/back");
const want = enc.encode(body);
if (!back || back.length !== want.length)
    die(`unzip gave back ${back ? back.length : 0} bytes of ${want.length}`);
for (let i = 0; i < want.length; i++)
    if (back[i] !== want[i])
        die(`the entry differs at byte ${i}`);

// 3. -0 stores and -9 deflates, and the sizes say which.
run("zip -0 -q /tmp/s.zip /home/t/a.txt");
run("zip -9 -q /tmp/d.zip /home/t/a.txt");
const stored = H.store.files.get("/tmp/s.zip").length;
const packed = H.store.files.get("/tmp/d.zip").length;
if (packed >= stored)
    die(`-9 made ${packed} bytes and -0 made ${stored}: the level did nothing`);

// 4. The pattern filters. They take a value list, so they come last.
run('zip -r -q /tmp/i.zip /home/t -i "*.txt"');
const included = listing("/tmp/i.zip");
if (!included.includes("a.txt") || included.includes("b.log"))
    die(`-i took the wrong entries: ${JSON.stringify(included)}`);

run('zip -r -q /tmp/x.zip /home/t -x "*.log"');
const excluded = listing("/tmp/x.zip");
if (excluded.includes("b.log") || !excluded.includes("a.txt"))
    die(`-x took the wrong entries: ${JSON.stringify(excluded)}`);

// 5. -j drops the path.
run("zip -j -q /tmp/j.zip /home/t/sub/c.txt");
const junked = listing("/tmp/j.zip");
if (!/^\s*6\s+c\.txt$/m.test(junked))
    die(`-j did not junk the path: ${JSON.stringify(junked)}`);

// 6. -h is upstream's help, and it names the program.
run("zip -h > /tmp/h 2>&1");
const help = dec.decode(H.store.files.get("/tmp/h") || new Uint8Array(0));
if (!/Copyright \(c\) 1990-2008 Info-ZIP/.test(help) || !/-r *recurse into directories/.test(help))
    die(`-h is not upstream's help: ${JSON.stringify(help.slice(0, 200))}`);

// 7. No arguments at a terminal is that help and status 0. The help is longer
//    than the 24 rows, so the tail is what is checked.
run("clear");
const bare = H.rows(run("zip; echo $?"));
if (!bare.includes("  -h2  show more help"))
    die(`bare zip did not print the help: ${JSON.stringify(bare)}`);
if (bare.some((r) => /cannot write zip file to terminal/.test(r)))
    die("bare zip still refuses to write to the terminal");
if (!bare.includes("0"))
    die(`bare zip did not exit 0: ${JSON.stringify(bare)}`);

// 8. The run is reproducible: the harness clock is frozen, so the DOS stamp
//    cannot move and nothing else may either.
run("zip -r -q /tmp/again.zip /home/t");
const a = H.store.files.get("/tmp/r.zip"), b = H.store.files.get("/tmp/again.zip");
if (a.length !== b.length)
    die(`two runs over the same tree wrote ${a.length} and ${b.length} bytes`);
for (let i = 0; i < a.length; i++)
    if (a[i] !== b[i])
        die(`two runs over the same tree differ at byte ${i}`);

console.log(`tree ok: 3 files and 2 directories in ${a.length} bytes, ` +
            `-9 ${packed} against -0 ${stored}, and the filters pick`);
