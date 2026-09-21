// Boot the kernel, plant the binary, start the clock with its dice pinned.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");

export const opt = {
    kernel: KERNEL,
    rootfs: ROOTFS,
    binary: join(APPS, "build/games/asciiclock/asciiclock.wasm"),
};

export let name = "asciiclock";

export function die(msg) {
    console.error(`${name}: ${msg}`);
    process.exit(1);
}

export let H;
const enc = new TextEncoder();
const dec = new TextDecoder();

export async function boot(caseName, cols = 80, rows = 24) {
    name = caseName;
    for (const [what, path, how] of [
        ["kernel", opt.kernel, "make"],
        ["rootfs", opt.rootfs, "make"],
        ["asciiclock", opt.binary, "make"],
    ]) {
        if (!existsSync(path)) {
            console.error(`${caseName}: no ${what} at ${path} — run \`${how}\``);
            process.exit(1);
        }
    }

    H = await import(HARNESS);
    await H.init(opt.kernel, opt.rootfs);
    H.kernel().init(0);
    if (H.run(0) !== -1) die("the kernel did not settle after boot");
    H.regrid(cols, rows, "resize returned no screen descriptor");
    if (!H.store.files.has("/bin/sh")) die("the archive did not unpack");
    H.store.files.set("/bin/asciiclock", new Uint8Array(readFileSync(opt.binary)));
    return H;
}

export let clock = 1;
export let delay = 40;

export function tick(n = 1) {
    for (let i = 0; i < n; i++) {
        H.run(clock);
        clock += delay;
    }
}

export function submit(line) {
    if (line.length > 59) die(`command line too long: ${line}`);
    H.type(line);
    H.press(H.KEY.ENTER);
    tick();
}

export function start() {
    submit("ASCIICLOCK_SEED=1 asciiclock");
}

export function press(k) {
    if (k in H.KEY) H.press(H.KEY[k]);
    else H.type(k);
    tick(2);
}

export function frame() {
    return H.rows(H.screen()).map((s) => s.replace(/\s+$/, "")).join("\n").replace(/\n+$/, "");
}

export function row(y) {
    return H.row(H.screen(), y).replace(/\s+$/, "");
}

export function fg_at(y, x) {
    return H.cell(H.screen(), x, y).fg & 0xf;
}

export function bg_at(y, x) {
    return H.cell(H.screen(), x, y).bg & 0xf;
}

export function get(path) {
    const b = H.store.files.get(path);
    return b === undefined ? null : dec.decode(b);
}

export function ok(what) {
    console.log(`${name} ok: ${what}`);
}
