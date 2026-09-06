// What every case here does: boot the kernel, plant mbasic, feed it a session
// with input and output redirected to files, and read the transcript back.
//
// A pipe rather than the grid, because a session does not fit on 24 rows -- and
// because down a pipe the interpreter asks neither MEMORY SIZE nor TERMINAL
// WIDTH and the LineEditor never runs, so nothing echoes and the transcript is
// exactly what BASIC printed. interrupt.mjs is the one case that needs the grid.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const CORE = resolve(APPS, "../braam-core");

export const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/lang/mbasic/mbasic.wasm"),
    bless: "",
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)(?:=(.*))?$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2] === undefined ? "1" : m[2];
}

export let name = "mbasic";

export function die(msg) {
    console.error(`${name}: ${msg}`);
    process.exit(1);
}

const enc = new TextEncoder();
const dec = new TextDecoder();

export let H;

// Checked before the harness is imported, because it exits the process itself
// and would not say what to build.
export async function boot(caseName) {
    name = caseName;
    for (const [what, path, how] of [
        ["kernel", opt.kernel, "make -C ../braam-core"],
        ["rootfs", opt.rootfs, "make -C ../braam-core"],
        ["mbasic", opt.binary, "make"],
    ]) {
        if (!existsSync(path)) {
            console.error(`${caseName}: no ${what} at ${path} — run \`${how}\``);
            process.exit(1);
        }
    }

    H = await import(join(CORE, "test/system/harness.mjs"));
    await H.init(opt.kernel, opt.rootfs);
    H.kernel().init(0);
    if (H.run(0) !== -1) die("the kernel did not settle after boot");
    H.regrid(80, 24, "resize returned no screen descriptor");
    if (!H.store.files.has("/bin/sh")) die("the archive did not unpack");
    // Planted, not packed: exec takes any path carrying a well-formed stamp,
    // and /bin is ordinary store content once boot has unpacked the archive.
    H.store.files.set("/bin/mb", new Uint8Array(readFileSync(opt.binary)));
    return H;
}

export function put(path, text) {
    H.store.files.set(path, enc.encode(text));
}

export function get(path) {
    const b = H.store.files.get(path);
    return b === undefined ? null : dec.decode(b);
}

let clock = 1;

// Run a session. The command line stays well under sixty characters: the
// harness keyboard is a Channel<Key, 64> and type() posts a whole line without
// checking.
export function session(lines) {
    put("/tmp/i", lines.join("\n") + "\n");
    const s = H.submit("mb </tmp/i >/tmp/o", (clock += 100));
    if (H.row(s, s.cursor_y) !== H.prompt())
        die("the shell did not get its prompt back at status 0");
    const out = get("/tmp/o");
    if (out === null) die("the program wrote nothing");
    return out;
}

// Everything up to and including the banner is the same in every case and
// says nothing about what is under test.
const HEAD = "\r\n65535 BYTES FREE\r\n\r\nBRAAM BASIC V1.1\r\n" +
             "COPYRIGHT 1978 MICROSOFT\r\n\r\nOK\r\n";

export function body(out) {
    if (!out.startsWith(HEAD))
        die(`the banner is not what INIT prints:\n${JSON.stringify(out.slice(0, 120))}`);
    return out.slice(HEAD.length);
}

// Byte for byte against the golden file beside the script. The run is
// deterministic -- RND is a fixed sequence and the clock is frozen -- so this
// is exact. Re-bless with --bless after reading the diff.
export function golden(file, text) {
    const path = join(HERE, file);
    if (opt.bless) {
        writeFileSync(path, text);
        console.error(`${name}: blessed ${file}`);
        return;
    }
    if (!existsSync(path)) die(`no golden at ${path} — run with --bless`);
    const want = readFileSync(path, "utf8");
    if (text === want) return;

    const a = want.split("\n"), b = text.split("\n");
    for (let i = 0; i < Math.max(a.length, b.length); i++)
        if (a[i] !== b[i])
            die(`${file} line ${i + 1}:\n  want ${JSON.stringify(a[i])}\n  got  ${JSON.stringify(b[i])}`);
    die(`${file} differs in length: want ${a.length} lines, got ${b.length}`);
}

export function ok() {
    console.log(`${name} ok`);
}
