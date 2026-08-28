// What every case here does: boot the kernel, plant em, drive it with keys,
// and read the screen back.
//
// Through the grid rather than a script, because that is what uemacs is: it
// has no command mode and no way to be driven down a pipe, so keys in and
// cells out is the whole of its interface.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const CORE = resolve(APPS, "../braam-core");

export const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/editors/uemacs/em.wasm"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

export let name = "em";

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
        ["em", opt.binary, "make"],
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
    H.store.files.set("/bin/em", new Uint8Array(readFileSync(opt.binary)));
    install();
    return H;
}

// The package as pkg would have installed it: em reads /pkg/bin/em to find its
// own share directory, so the whole link chain has to be real. The leaf may
// dangle -- readlink does not follow it -- and the binary stays at /bin/em.
export const STORE = "/pkg/store/uemacs-4.0-r1";

function install() {
    for (const f of ["emacs.rc", "emacs.hlp"])
        H.store.files.set(`${STORE}/share/${f}`,
                          new Uint8Array(readFileSync(join(HERE, "..", f))));
    /* Under sixty characters each: type() posts a whole line into a 64-key
       channel without checking. */
    submit(`mkdir -p ${STORE}/bin /pkg/gen/1/bin`);
    submit(`ln -s ${STORE}/bin/em /pkg/gen/1/bin/em`);
    submit("ln -s /pkg/gen/1 /pkg/active");
    submit("ln -s /pkg/active/bin /pkg/bin");
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

// Leave the session that is up: a second em cannot be submitted while the
// first still holds the screen.
//
// C-u 0 C-x C-c, which is upstream's hard quit: ^C arrives as SIG_INT and the
// editor hands the keystroke back, so the chord works.  The argument forces it
// -- a bare exit asks about a changed buffer, and an answer typed to a question
// that was never asked lands on the shell's next command line.
export function quit() {
    if (!running)
        return;
    press("^g");
    press("^u");
    press("0");
    press("^x");
    press("^c");
    /* The shell gets its screen back when the claim drops, and redraws its
       prompt a tick later. */
    tick(2);
    running = false;
}

// Any shell line, for the cases that need one -- a cd before em, say. The
// clock lives here, so it cannot be driven from outside the module.
export function submit(line) {
    quit();
    running = true;
    H.submit(line, clock++);
    tick(6);
}

export function em(args = "", keys = []) {
    quit();
    running = true;
    H.submit(`em ${args}`, clock++);
    /* Enough for the shell to spawn it, for it to claim the screen, and for
       the packaged emacs.rc to run: a key pressed before that lands on the
       shell's command line instead. */
    tick(6);
    for (const k of keys)
        press(k);
    return screen();
}

const NAMED = { CR: "ENTER", ESC: "ESCAPE" };

const name_of = (k) => NAMED[k] || k;

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

// The mode line, which is the row above the message line.
export const modeline = () => row(22);

export function cursor() {
    const s = H.screen();
    return `${s.cursor_x},${s.cursor_y}`;
}

const COLORS = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"];

export function fg(y) {
    const c = H.cell(H.screen(), 0, y).fg;
    return COLORS[c & 7] + (c & 8 ? "+" : "");
}

// The attribute byte, which harness.mjs's cell() does not unpack: the mode
// line is the one row that carries one, and reverse video is what says so.
export function attrs(y, x = 0) {
    const s = H.screen();
    return (H.mem.u32()[(s.cells + (y * s.cols + x) * 8) / 4 + 1] >>> 16) & 0xff;
}

export const ATTR_REVERSE = 4;

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
