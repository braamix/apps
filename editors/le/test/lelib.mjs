// What every case here does: boot the kernel, plant le, drive it with keys,
// and read the screen back.
//
// Through the grid, because that is what LE is: it has no command mode and no
// way to be driven down a pipe, so keys in and cells out is the whole of its
// interface.

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const CORE = resolve(APPS, "../braam-core");

export const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/editors/le/le.wasm"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

export let name = "le";

export function die(msg) {
    console.error(`${name}: ${msg}`);
    process.exit(1);
}

const enc = new TextEncoder();
const dec = new TextDecoder();

export let H;

export async function boot(caseName) {
    name = caseName;
    for (const [what, path, how] of [
        ["kernel", opt.kernel, "make -C ../braam-core"],
        ["rootfs", opt.rootfs, "make -C ../braam-core"],
        ["le", opt.binary, "make"],
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
    H.store.files.set("/bin/le", new Uint8Array(readFileSync(opt.binary)));
    install();
    return H;
}

// The package as pkg would have installed it: le reads /pkg/bin/le to find its
// own share directory, so the whole link chain has to be real. The leaf may
// dangle -- readlink does not follow it -- and the binary stays at /bin/le.
export const STORE = "/pkg/store/le-1.16.8-r0";

function install() {
    for (const f of ["le.hlp", "keymap", "mainmenu", "syntax"])
        H.store.files.set(`${STORE}/share/${f}`,
                          new Uint8Array(readFileSync(join(HERE, "../share", f))));
    /* The syntax file includes these by name; without them nothing highlights. */
    for (const f of readdirSync(join(HERE, "../share/syntax.d")))
        H.store.files.set(`${STORE}/share/syntax.d/${f}`,
                          new Uint8Array(readFileSync(join(HERE, "../share/syntax.d", f))));
    /* Under sixty characters each: type() posts a whole line into a 64-key
       channel without checking. */
    submit(`mkdir -p ${STORE}/bin /pkg/gen/1/bin`);
    submit(`ln -s ${STORE}/bin/le /pkg/gen/1/bin/le`);
    submit("ln -s /pkg/gen/1 /pkg/active");
    submit("ln -s /pkg/active/bin /pkg/bin");
    submit("mkdir -p /home/.le");
}

export function put(path, text) {
    H.store.files.set(path, enc.encode(text));
}

export function get(path) {
    const b = H.store.files.get(path);
    return b === undefined ? null : dec.decode(b);
}

export let clock = 1;

let running = false;

// Leave the session that is up: a second le cannot be submitted while the
// first still holds the screen.
//
// ESC is LE's quit, and it asks about a modified buffer -- the menu's default
// button is Yes, so N says no and leaves without saving.
export function quit() {
    if (!running)
        return;
    press("ESC");
    press("n");
    /* The shell gets its screen back when the claim drops, and redraws its
       prompt a tick later. */
    tick(3);
    running = false;
}

// Any shell line, for the cases that need one -- a mkdir before le, say. The
// clock lives here, so it cannot be driven from outside the module.
//
// This does not mark a session running: only le() does. Upstream's quit chord
// was control characters the shell ignored; LE's is ESC and a letter, and a
// letter typed at a shell prompt is a command.
export function submit(line) {
    quit();
    H.submit(line, clock++);
    tick(6);
}

export function le(args = "", keys = []) {
    quit();
    running = true;
    H.submit(`le ${args}`, clock++);
    /* Enough for the shell to spawn it, for it to claim the screen, and for
       the config files to be read: a key pressed before that lands on the
       shell's command line instead. */
    tick(8);
    for (const k of keys)
        press(k);
    return screen();
}

const NAMED = { CR: "ENTER", ESC: "ESCAPE" };

const name_of = (k) => NAMED[k] || k;

// MOD_META from key.h: the Command key, which harness.mjs does not name.
export const CMD = 8;

// F1 to F12, which harness.mjs's KEY table stops short of. They are the last
// twelve of key.h's enum, after PAGE_DOWN.
const fkey = (k) => (/^F([1-9]|1[0-2])$/.test(k) ? H.KEY.PAGE_DOWN + Number(k.slice(1)) : 0);

// One key.  A string is typed as itself; "^x" is control-x and "^LEFT" a named
// key with control held.  Each is followed by a run, so the editor has
// answered before the next arrives.
export function press(k) {
    const ctrl = k.startsWith("^") ? name_of(k.slice(1)) : "";
    if (ctrl in H.KEY && ctrl)
        H.press(H.KEY[ctrl], H.CTRL);
    else if (k.startsWith("^") && k.length === 2)
        H.press(k[1].toLowerCase().codePointAt(0), H.CTRL);
    else if (name_of(k) in H.KEY)
        H.press(H.KEY[name_of(k)]);
    else if (fkey(k))
        H.press(fkey(k));
    else
        H.type(k);
    H.run(clock++);
}

export function keys(...ks) {
    for (const k of ks)
        press(k);
}

export function tick(n = 1) {
    for (let i = 0; i < n; i++)
        H.run(clock++);
}

export function screen(rows = 0) {
    const r = H.rows(H.screen()).map((s) => s.replace(/\s+$/, ""));
    return (rows ? r.slice(0, rows) : r).join("\n").replace(/\n+$/, "");
}

// One row, by number, with the trailing blanks off. screen() drops the empty
// rows at the bottom, so the mode line is not reliably the last of them.
export function row(y) {
    return H.rows(H.screen())[y].replace(/\s+$/, "");
}

// The status line, which LE puts on the bottom row -- whatever row that is
// after a resize.
export const status = () => row(H.rows(H.screen()).length - 1);

export function cursor() {
    const s = H.screen();
    return `${s.cursor_x},${s.cursor_y}`;
}

const COLORS = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"];

const color_of = (c) => COLORS[c & 7] + (c & 8 ? "+" : "");

export function fg(y) {
    return color_of(H.cell(H.screen(), 0, y).fg);
}

export function bg(y) {
    return color_of(H.cell(H.screen(), 0, y).bg);
}


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

export function regrid(cols, rows) {
    H.regrid(cols, rows, "resize returned no screen descriptor");
    tick(2);
}
