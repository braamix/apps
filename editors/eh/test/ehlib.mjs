// Boot the kernel, plant eh, feed it a keystroke script, and read back the
// screen and the file. Upstream drove the same scripts through a pipe into
// curses' getch(); keys come from the screen here, so they are pressed.

import { existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const CORE = resolve(APPS, "../braam-core");

export const opt = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/editors/eh/eh.wasm"),
};
for (const a of process.argv.slice(2)) {
    const m = /^--(\w+)=(.*)$/.exec(a);
    if (m && m[1] in opt) opt[m[1]] = m[2];
}

export let name = "eh";

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
        ["eh", opt.binary, "make"],
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
    H.store.files.set("/bin/eh", new Uint8Array(readFileSync(opt.binary)));
    return H;
}

export function put(path, text) {
    H.store.files.set(path, typeof text === "string" ? enc.encode(text) : text);
}

export function get(path) {
    const b = H.store.files.get(path);
    return b === undefined ? null : dec.decode(b);
}

export function rm(path) {
    H.store.files.delete(path);
}

export let clock = 1;

export function tick(n = 1) {
    for (let i = 0; i < n; i++) H.run(clock++);
}

export function submit(line) {
    H.submit(line, clock++);
}

// The shell is back at a prompt and nothing else holds the screen. A case whose
// script left the editor up would otherwise take the next case's keystrokes.
export function at_prompt() {
    const rows = H.rows(H.screen()).filter((r) => r.trim() !== "");
    const last = rows.length ? rows[rows.length - 1] : "";
    return last.endsWith("$");
}

// Quit whatever is still up, however it is asking. ESC leaves insert or a
// field, Q asks to quit, y discards.
export function settle() {
    for (let i = 0; i < 8 && !at_prompt(); i++) {
        for (const k of ["\x1b", "Q", "y", "\n"]) press(k);
        tick(2);
    }
    tick(2);
    return at_prompt();
}

export function chdir(path) {
    H.chdir(path);
}

// One key per run, which keeps the order deterministic and the harness's
// Channel<Key, 64> from ever filling.
const NAMED = {
    "\n": "ENTER",
    "\r": "ENTER",
    "\x1b": "ESCAPE",
    "\x7f": "BACKSPACE",
    "\b": "BACKSPACE",
    "\t": "TAB",
};

export function press(ch) {
    const n = NAMED[ch];
    if (n) H.press(H.KEY[n]);
    // press() takes a codepoint, not a character: a string here reaches the
    // kernel as key 0 and getch() drops it, so every ^X was a lost keystroke.
    else if (ch < " ") H.press(ch.charCodeAt(0) + 96, H.CTRL);
    else H.type(ch);
    H.run(clock++);
}

// A script is a string of raw bytes, as upstream's printf produced. Bytes past
// ASCII are a UTF-8 sequence: decode it so one keypress is one codepoint.
function runes(script) {
    const b = [];
    for (let i = 0; i < script.length; i++) b.push(script.charCodeAt(i) & 0xff);
    return Array.from(dec.decode(new Uint8Array(b)));
}

export function keys(script) {
    for (const ch of runes(script)) press(ch);
}

// The same, answering the last frame drawn while the editor still owned the
// screen. Upstream's golden is a trace of every frame, and its end state is the
// last one eh emitted -- by the time the script's Q has been processed the
// shell has the screen back.
export function keys_watching(script, snapshot) {
    let last = snapshot();
    let live = !prompt_in(last);
    let gone = false;

    for (const ch of runes(script)) {
        press(ch);
        // One read of the grid per key, not two: whether the editor still owns
        // the screen is decided from the snapshot itself.
        const now = snapshot();
        const at = prompt_in(now);

        // Latched: a script may have keys left over after the editor quits --
        // del2's trailing `y` answers a "discard changes" that a clean buffer
        // never asks -- and the shell echoes them, so the prompt test alone
        // would start matching the editor again.
        if (live && at) gone = true;
        if (!at) live = true;
        if (!at && !gone) last = now;
    }
    return last;
}

// The shell's prompt is the last non-blank line of an image's text half.
export function prompt_in(img) {
    const text = img.split("\n---\n")[0];
    const rows = text.split("\n").filter((r) => r.trim() !== "");
    const lastRow = rows.length ? rows[rows.length - 1] : "";
    return lastRow.endsWith("$");
}

// The screen as upstream's replayed goldens are shaped: 24 rows of text, then a
// mask with '#' where ATTR_REVERSE is set.
//
// The whole grid in one view rather than a call per cell: this runs once per
// keystroke over every case, and H.cell() would be a million calls. Cell is
// { char32_t ch; u8 fg, bg, attrs; } -- two words, attrs in the third byte of
// the second. H.cell() itself reports ch, fg and bg but not attrs.
const ATTR_REVERSE = 4;

export function image() {
    const s = H.screen();
    const w = H.mem.u32().subarray(s.cells / 4, s.cells / 4 + s.rows * s.cols * 2);
    const text = [];
    const mask = [];

    for (let y = 0; y < s.rows; y++) {
        let t = "";
        let m = "";
        for (let x = 0; x < s.cols; x++) {
            const i = (y * s.cols + x) * 2;
            const ch = w[i];
            t += ch ? String.fromCodePoint(ch) : " ";
            m += (w[i + 1] >>> 16) & ATTR_REVERSE ? "#" : ".";
        }
        text.push(t.replace(/\s+$/, ""));
        mask.push(m.replace(/\.+$/, ""));
    }
    return text.join("\n") + "\n---\n" + mask.join("\n");
}

export function screen() {
    return H.rows(H.screen())
        .map((s) => s.replace(/\s+$/, ""))
        .join("\n")
        .replace(/\n+$/, "");
}

export function cursor() {
    const s = H.screen();
    return `${s.cursor_x},${s.cursor_y}`;
}

export function is(what, got, wanted) {
    const trim = (s) => String(s).replace(/[ \t]+$/gm, "").replace(/\n+$/, "");
    if (trim(got) !== trim(wanted)) {
        console.error(`${name}: ${what}`);
        console.error("--- got ---\n" + got);
        console.error("--- want ---\n" + wanted);
        process.exit(1);
    }
}

export function ok(what) {
    console.log(`${name} ok: ${what}`);
}
