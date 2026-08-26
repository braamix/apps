// What iconv does when it cannot convert: the exit statuses, the messages, and
// -c. Upstream's behaviour, which is easy to lose in a port because none of it
// is on the happy path.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/converters/iconv/iconv.wasm"),
    data: join(APPS, "build/converters/iconv/i18n"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}
for (const [what, path, how] of [
    ["kernel", opt.kernel, "make -C ../braam-core"],
    ["rootfs", opt.rootfs, "make -C ../braam-core"],
    ["iconv", opt.binary, "make"],
    ["the i18n tree", opt.data, "make"],
]) {
    if (!existsSync(path)) {
        console.error(`errors: no ${what} at ${path} — run \`${how}\``);
        process.exit(1);
    }
}

const H = await import(join(CORE, "test/system/harness.mjs"));

function die(msg) {
    console.error("errors: " + msg);
    process.exit(1);
}

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1) die("the kernel did not settle after boot");
H.regrid(80, 24, 8, 16);
if (!H.store.files.has("/bin/sh")) die("the archive did not unpack");

H.store.files.set("/bin/iconv", new Uint8Array(readFileSync(opt.binary)));
(function plant(dir, rel) {
    for (const name of readdirSync(dir)) {
        const from = join(dir, name);
        const at = rel ? rel + "/" + name : name;
        if (statSync(from).isDirectory()) plant(from, at);
        else H.store.files.set("/opt/share/i18n/" + at, new Uint8Array(readFileSync(from)));
    }
})(opt.data, "");

let now = 1;
function run(cmd) {
    if (cmd.length > 60) die(`the command line is ${cmd.length} chars; the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1) die(`the kernel did not settle after: ${cmd}`);
}
const read = (p) => H.store.files.get(p) ?? new Uint8Array();
const text = (p) => new TextDecoder().decode(read(p));
const status = () => text("/s").trim();

// Runs a command and records `$?`, which is what most of this is about.
function check(cmd) {
    run(`${cmd}; echo $? >/s`);
    return status();
}

run("export ICONV_PREFIX=/opt");

let checked = 0;
function want(what, got, expected) {
    if (got !== expected) die(`${what}: got ${JSON.stringify(got)}, want ${JSON.stringify(expected)}`);
    checked++;
}

// 1. No arguments at all is a usage error.
{
    const st = check("iconv >/o 2>/e");
    want("no arguments exits 1", st, "1");
    if (!text("/e").startsWith("Usage:"))
        die(`no arguments should print the usage, got ${JSON.stringify(text("/e"))}`);
    if (!text("/e").includes("--help"))
        die(`the usage should point at --help, got ${JSON.stringify(text("/e"))}`);
}

// 2. An encoding that does not exist.
{
    H.store.files.set("/i", new TextEncoder().encode("hello\n"));
    const st = check("iconv -f NOPE -t UTF-8 </i >/o 2>/e");
    want("an unknown encoding exits 1", st, "1");
    const e = text("/e");
    if (!e.includes("iconv_open"))
        die(`an unknown encoding should name iconv_open, got ${JSON.stringify(e)}`);
}

// 3. An illegal sequence: 0xFF is not UTF-8. Without -c that is an error;
//    with it the character is dropped and the rest still converts.
{
    H.store.files.set("/i", new Uint8Array([0x41, 0xff, 0x42, 0x0a]));
    const st = check("iconv -f UTF-8 -t ASCII </i >/o 2>/e");
    if (st === "0")
        die("an illegal sequence should not exit 0");
    checked++;

    const st2 = check("iconv -c -f UTF-8 -t ASCII </i >/p 2>/f");
    want("-c exits 0", st2, "0");
    const got = read("/p");
    const kept = [...got].map((b) => String.fromCharCode(b)).join("");
    if (kept !== "AB\n")
        die(`-c should keep the convertible bytes, got ${JSON.stringify(kept)}`);
}

// 4. A file that is not there is a warning, and the operands after it are
//    still converted — upstream demotes this for conformance.
{
    H.store.files.set("/a", new TextEncoder().encode("one\n"));
    const st = check("iconv -f UTF-8 -t UTF-8 /z /a >/o 2>/e");
    want("a missing operand exits 1", st, "1");
    if (text("/o") !== "one\n")
        die(`the operands after a missing one should still convert, got ${JSON.stringify(text("/o"))}`);
    if (!text("/e").includes("/z"))
        die(`the missing file should be named, got ${JSON.stringify(text("/e"))}`);
}

// 5. -l refuses to be combined with the other flags.
{
    const st = check("iconv -l -f UTF-8 >/o 2>/e");
    want("-l with -f exits 1", st, "1");
    if (!text("/e").includes("-l is not allowed"))
        die(`-l with -f should say so, got ${JSON.stringify(text("/e"))}`);
}

// 6. An incomplete character at the end of the input.
{
    // 0xE4 begins a three-byte sequence that never arrives.
    H.store.files.set("/i", new Uint8Array([0x41, 0xe4]));
    const st = check("iconv -f UTF-8 -t UTF-8 </i >/o 2>/e");
    want("a truncated character exits 1", st, "1");
    if (!text("/e").includes("incomplete"))
        die(`a truncated character should say so, got ${JSON.stringify(text("/e"))}`);
}

// 7. The long options upstream declares.
{
    H.store.files.set("/i", new TextEncoder().encode("ok\n"));
    const st = check("iconv --from-code=UTF-8 --to-code=ASCII </i >/o");
    want("--from-code and --to-code exit 0", st, "0");
    if (text("/o") !== "ok\n")
        die(`the long options should convert, got ${JSON.stringify(text("/o"))}`);
}

// 8. --help explains the options on stdout, and -h is the same thing.
{
    const st = check("iconv --help >/o 2>/e");
    want("--help exits 0", st, "0");
    const h = text("/o");
    if (!h.startsWith("Usage:") || !h.includes("Options:") || !h.includes("Examples:"))
        die(`--help should explain the options, got ${JSON.stringify(h)}`);
    if (text("/e") !== "")
        die(`--help should write nothing to stderr, got ${JSON.stringify(text("/e"))}`);

    const st2 = check("iconv -h >/p 2>/f");
    want("-h exits 0", st2, "0");
    if (text("/p") !== h)
        die("-h and --help should print the same text");
}

console.log(`errors ok: ${checked} behaviours`);
