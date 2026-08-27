// What every case here does: boot the kernel, plant ex, run a script of ex
// commands with input and output redirected to files, and read the result back.
//
// A script rather than the grid, because that is what ex is: a long transcript
// does not fit on 24 rows, and command mode prints no prompt over a pipe, which
// is exactly what makes a transcript assertable.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const CORE = resolve(APPS, "../braam-core");

export const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/editors/vi/ex.wasm"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

export let name = "ex";

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
        ["ex", opt.binary, "make"],
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
    H.store.files.set("/bin/ex", new Uint8Array(readFileSync(opt.binary)));
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

// Run `ex <file>` with `script` on its standard input and its output in /tmp/o.
// The command line stays well under sixty characters: the harness keyboard is a
// Channel<Key, 64> and type() posts a whole line without checking.
export function ex(file, script, args = "") {
    put("/tmp/c", script);
    H.store.files.delete("/tmp/o");
    const line = `ex ${args}${file} </tmp/c >/tmp/o`;
    if (line.length > 59) die(`command line too long: ${line}`);
    H.submit(line, clock++);
    H.run(clock++);
    H.run(clock++);
    return get("/tmp/o") ?? "";
}

// Compare, and print both sides when they differ. Trailing blanks are stripped
// per line so that a later change of width does not churn the expectations.
export function is(what, got, want) {
    const trim = (s) => s.replace(/[ \t]+$/gm, "").replace(/\n+$/, "");
    if (trim(got) !== trim(want)) {
        console.error(`${name}: ${what}`);
        console.error("--- got ---\n" + got);
        console.error("--- want ---\n" + want);
        process.exit(1);
    }
}

export function ok(what) {
    console.log(`${name} ok: ${what}`);
}
