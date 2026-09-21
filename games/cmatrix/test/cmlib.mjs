// What every case here does: boot the kernel, plant the binary, start the
// rain with its dice pinned, and drive it a frame at a time.
//
// The frame clock is update*10 ms (40 by default), so one tick of the
// harness clock is one frame; CMATRIX_SEED is what makes the sequence the
// same every run, and nothing in the program reads proc_now(), which the
// harness freezes.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");

export const opt = {
    kernel: KERNEL,
    rootfs: ROOTFS,
    binary: join(APPS, "build/games/cmatrix/cmatrix.wasm"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

export let name = "cmatrix";

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
        ["cmatrix", opt.binary, "make"],
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
    H.store.files.set("/bin/cmatrix", new Uint8Array(readFileSync(opt.binary)));
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

export function start(args = "") {
    const line = `CMATRIX_SEED=1 cmatrix${args ? " " + args : ""}`;
    submit(line);
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

export function hue(y) {
    const s = H.screen();
    let out = "";
    for (let x = 0; x < s.cols; x++)
        out += (H.cell(s, x, y).fg & 0xf).toString(16);
    return out;
}

export function fg_at(y, x) {
    return H.cell(H.screen(), x, y).fg & 0xf;
}

export function get(path) {
    const b = H.store.files.get(path);
    return b === undefined ? null : dec.decode(b);
}

export function put(path, text) {
    H.store.files.set(path, enc.encode(text));
}

export function regrid(cols, rows) {
    H.regrid(cols, rows, "resize returned no screen descriptor");
    tick(2);
}

export function ok(what) {
    console.log(`${name} ok: ${what}`);
}
