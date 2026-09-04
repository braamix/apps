// What every case here does: boot the kernel, plant the binary, start the
// aquarium with its dice pinned, and drive it a frame at a time.
//
// The frame clock is a 100 ms sleep, so one tick of the harness clock is one
// frame; ASCIIQUARIUM_SEED is what makes the sequence the same every run, and
// nothing in the program reads proc_now(), which the harness freezes.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const CORE = resolve(APPS, "../braam-core");

export const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/games/asciiquarium/asciiquarium.wasm"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

export let name = "asciiquarium";

export function die(msg) {
    console.error(`${name}: ${msg}`);
    process.exit(1);
}

export let H;

// Checked before the harness is imported, since it exits the process itself
// and would not say what to build.
export async function boot(caseName, cols = 80, rows = 24) {
    name = caseName;
    for (const [what, path, how] of [
        ["kernel", opt.kernel, "make -C ../braam-core"],
        ["rootfs", opt.rootfs, "make -C ../braam-core"],
        ["asciiquarium", opt.binary, "make"],
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
    H.regrid(cols, rows, "resize returned no screen descriptor");
    if (!H.store.files.has("/bin/sh")) die("the archive did not unpack");
    H.store.files.set("/bin/asciiquarium", new Uint8Array(readFileSync(opt.binary)));
    return H;
}

export let clock = 1;

// One frame: the sleep is 100 ms, so that is what a tick is worth.
export function tick(n = 1) {
    for (let i = 0; i < n; i++) {
        H.run(clock);
        clock += 100;
    }
}

// The command line stays well under sixty characters: the harness keyboard is
// a Channel<Key, 64> and type() posts a whole line without checking.
export function start(args = "") {
    const line = `ASCIIQUARIUM_SEED=1 asciiquarium${args ? " " + args : ""}`;
    if (line.length > 59) die(`command line too long: ${line}`);
    H.type(line);
    H.press(H.KEY.ENTER);
    tick();
}

// A key, and the two frames it takes to land: the first wakes the keyboard
// task, the second is the frame the clock draws after it.
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

// One row's foreground colours, a hex digit a cell: a Braam cell has sixteen.
export function hue(y) {
    const s = H.screen();
    let out = "";
    for (let x = 0; x < s.cols; x++)
        out += (H.cell(s, x, y).fg & 0xf).toString(16);
    return out.replace(/7+$/, "");
}

export function regrid(cols, rows) {
    H.regrid(cols, rows, "resize returned no screen descriptor");
    tick(2);
}

export function is(what, got, want) {
    const trim = (s) => String(s).replace(/[ \t]+$/gm, "").replace(/\n+$/, "");
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
