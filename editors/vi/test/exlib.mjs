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
    visual: join(APPS, "build/editors/vi/vi.wasm"),
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
        ["vi", opt.visual, "make"],
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
    H.store.files.set("/bin/vi", new Uint8Array(readFileSync(opt.visual)));
    return H;
}

export function put(path, text) {
    H.store.files.set(path, enc.encode(text));
}

export function get(path) {
    const b = H.store.files.get(path);
    return b === undefined ? null : dec.decode(b);
}

export let clock = 1;

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

// ------------------------------------------------------------------- visual
//
// The other half is driven through the grid, because that is what visual mode
// is: keys in, cells out. A screen is asserted as its rows joined with
// newlines, trailing blanks stripped, so that a later change of width does not
// churn the expectations.

/* Two names everyone writes and the harness does not have. */
const H_KEY_ALIASES = { CR: "ENTER", ESC: "ESCAPE" };

let running = false;

// Leave the session that is up, if there is one: a second vi cannot be
// submitted while the first still holds the screen.
export function quitvi() {
    if (!running)
        return;
    press("ESC");
    press(":");
    H.type("q!");
    press("CR");
    /* The screen the shell had is restored when the claim drops, and its
       prompt is redrawn a tick later. */
    H.run(clock++);
    H.run(clock++);
    running = false;
}

export function vi(file, keys = []) {
    quitvi();
    running = true;
    H.submit(`vi ${file}`, clock++);
    H.run(clock++);
    for (const k of keys)
        press(k);
    return screen();
}

// One key. A string is typed as itself; "^x" is control-x and "^LEFT" is a
// named key with control held; the named keys go by name. Each is followed by
// a run, so the editor has answered before the next one arrives.
const NAMED = { ...H_KEY_ALIASES };

const name_of = (k) => NAMED[k] || k;

export function press(k) {
    const ctrl = k.startsWith("^") ? name_of(k.slice(1)) : "";
    if (ctrl in H.KEY && ctrl)
        H.press(H.KEY[ctrl], H.CTRL);
    else if (k.startsWith("^") && k.length === 2)
        H.press(k[1].toLowerCase().codePointAt(0), H.CTRL);
    else if (name_of(k) in H.KEY)
        H.press(H.KEY[name_of(k)]);
    else
        H.type(k);
    H.run(clock++);
}

export function screen(rows = 0) {
    const r = H.rows(H.screen()).map((s) => s.replace(/\s+$/, ""));
    return (rows ? r.slice(0, rows) : r).join("\n").replace(/\n+$/, "");
}

// Where the cursor is on the screen, as "x,y". Worth asserting on its own: it
// rides in the blit's header, and a frame with no damaged cell is not sent at
// all, so a motion -- which changes no cell -- is exactly what goes missing.
export function cursor() {
    const s = H.screen();
    return `${s.cursor_x},${s.cursor_y}`;
}

const COLORS = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"];

// The foreground colour of a row, named: the ~ rows and the echo line are not
// white, and nothing else says so.
export function fg(y) {
    const c = H.cell(H.screen(), 0, y).fg;
    return COLORS[c & 7] + (c & 8 ? "+" : "");
}

export function regrid(cols, rows) {
    H.regrid(cols, rows, "resize returned no screen descriptor");
    H.run(clock++);
    H.run(clock++);
}
