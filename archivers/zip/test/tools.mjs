// The other three commands, and encryption, headless.
//
// zipnote and zipsplit read an archive and write it back; zipcloak asks for a
// password at the terminal, which is as far as a redirected run gets — that is
// upstream's rule too, and reaching the prompt is what proves it parsed its
// arguments and read the archive.
//
// Encryption itself is checked through zip -P, whose output a real Info-ZIP
// unzip decrypts; Braam's own unzip refuses it, which is Package_Formats.md
// §5.2 turning down flag bit 0 and is the right answer.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    bindir: join(APPS, "build/archivers/zip"),
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|bindir)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: tools.mjs [--kernel=<wasm>] [--rootfs=<zip>] [--bindir=<dir>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`tools: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
const TOOLS = ["zip", "zipnote", "zipsplit", "zipcloak"];
for (const t of TOOLS)
    if (!existsSync(join(opt.bindir, t + ".wasm")))
        die(`no ${t}.wasm in ${opt.bindir} — run make`);

const H = await import(join(CORE, "test/system/harness.mjs"));

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
for (const t of TOOLS)
    H.store.files.set(`/bin/${t}`, new Uint8Array(readFileSync(join(opt.bindir, t + ".wasm"))));

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
const flat = (s) => H.rows(s).join(" ").replace(/\s+/g, " ");

let body = "";
for (let i = 0; i < 200; i++)
    body += `line ${i} of some text to make this worth splitting\n`;
H.store.files.set("/home/a", enc.encode(body));
run("echo beta > /home/b");
run("zip -q /home/x.zip /home/a /home/b");

// 1. zipnote lists every entry, each with the marker upstream uses.
const noted = flat(run("zipnote /home/x.zip"));
for (const want of ["@ home/a", "@ home/b", "(comment above this line)",
                    "(zip file comment below this line)"])
    if (!noted.includes(want))
        die(`zipnote did not print ${JSON.stringify(want)}: ${JSON.stringify(noted)}`);

// 2. zipsplit breaks the archive up, and says what it made.
const split = flat(run("zipsplit -n 3000 /home/x.zip"));
if (!/zip files will be made/.test(split))
    die(`zipsplit said nothing about what it would make: ${JSON.stringify(split)}`);
const parts = [...H.store.files.keys()].filter((p) => /^\/home\/x\d+\.zip$/.test(p));
if (parts.length === 0)
    die("zipsplit made no parts");
for (const p of parts) {
    run(`unzip -l ${p} > /tmp/l 2>&1`);
    const out = dec.decode(H.store.files.get("/tmp/l") || new Uint8Array(0));
    if (/error|cannot|invalid/i.test(out))
        die(`unzip would not read ${p}: ${JSON.stringify(out)}`);
}

// 3. zip -P encrypts, and Braam's own unzip refuses what it wrote: §5.2 turns
//    down an entry with flag bit 0, which is exactly what this is.
run("zip -q /home/p.zip /home/b");
run("zip -q -P hunter2 /home/e.zip /home/b");
const enc_zip = H.store.files.get("/home/e.zip");
const plain = H.store.files.get("/home/p.zip");
if (!enc_zip || !enc_zip.length)
    die("zip -P wrote no archive");
// Bit 0 of the local header's general purpose flag is what says the entry is
// encrypted; it sits two bytes after the version-needed field.
const flag = enc_zip[6] | (enc_zip[7] << 8);
if (!(flag & 1))
    die(`the entry is not marked encrypted: flag ${flag}`);
if ((plain[6] | (plain[7] << 8)) & 1)
    die("the unencrypted entry is marked encrypted");
// Twelve bytes of encryption header, and the data descriptor it forces.
if (enc_zip.length !== plain.length + 12 + 16)
    die(`the encrypted archive is ${enc_zip.length} bytes against ${plain.length}: ` +
        `expected twelve of header and sixteen of descriptor`);
run("unzip -p /home/e.zip home/b > /tmp/p 2>/tmp/pe");
const why = dec.decode(H.store.files.get("/tmp/pe") || new Uint8Array(0));
if (!/unsupported/.test(why))
    die(`unzip should have refused an encrypted entry, and said ${JSON.stringify(why)}`);

// 4. zipcloak asks for a password. It reads one from the terminal only, so a
//    run whose input is redirected gets no further — and that is upstream's.
//    It goes last: it parks on the keyboard holding the claim, and anything
//    submitted after it would be typed at its prompt rather than the shell's.
const asked = flat(run("zipcloak /home/x.zip"));
if (!/Enter password:/.test(asked))
    die(`zipcloak did not reach its prompt: ${JSON.stringify(asked)}`);

console.log(`tools ok: zipnote lists ${2} entries, zipsplit made ${parts.length}, ` +
            `zipcloak prompts, and -P is refused by a reader that may not read it`);
