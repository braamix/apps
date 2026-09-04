// -c paints the same fluid in colour: the glyphs are the black-and-white run's
// frame, and the backgrounds are the density.
//
// colour.log is a row of glyphs and then a row of one hex digit a cell — the
// sixteen colours a Braam cell has, which is what upstream's 6x6x6 cube index
// quantises to on the way in.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/games/asciifluid/asciifluid.wasm"),
    // The frame the golden holds; the fluid has moved by then.
    frame: "5",
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary|frame)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: colour.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>] [--frame=<n>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}
const AT = Number(opt.frame);

const die = (msg) => {
    console.error(`asciifluid: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

const H = await import(join(CORE, "test/system/harness.mjs"));

const LOG = join(APPS, "build/games/asciifluid/colour.log");

// ---------------------------------------------------------------- the run

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/asciifluid", new Uint8Array(readFileSync(opt.binary)));
H.store.files.set("/tmp/c",
                  new Uint8Array(readFileSync(join(APPS, "games/asciifluid/data/column.txt"))));

H.type("asciifluid -c /tmp/c");
H.press(H.KEY.ENTER);

// The opening frame stands for a second, the rest for twelve milliseconds.
let now = 1;
for (let k = 0; k <= AT; k++) {
    H.run(now);
    now += k ? 12.5 : 1001;
}

// The frame as glyphs and as colours. The program paints 79 of the 80 columns.
const s = H.screen();
const lines = [];
for (let y = 0; y < s.rows; y++) {
    let text = "", ink = "";
    for (let x = 0; x < 79; x++) {
        const c = H.cell(s, x, y);
        text += c.ch ? String.fromCodePoint(c.ch) : " ";
        ink += (c.bg & 0xf).toString(16);
    }
    lines.push(text.replace(/ +$/, ""), ink);
}
const got = lines.join("\n") + "\n";
mkdirSync(dirname(LOG), { recursive: true });
writeFileSync(LOG, got);

const fail = (msg) => {
    console.error(`asciifluid: ${msg}`);
    console.error(`asciifluid: the frame is ${LOG}`);
    process.exit(1);
};

// ------------------------------------------------------------- assertions

// 1. -c is a renderer, not a different simulation: the glyphs are the ones the
//    black-and-white run painted at the same frame.
const bw = readFileSync(join(HERE, "frames.log"), "utf8").split("\n");
const at = bw.indexOf(`--- frame ${AT}`);
if (at < 0)
    fail(`frames.log has no frame ${AT}`);
for (let y = 0; y < s.rows; y++)
    if (lines[y * 2] !== bw[at + 1 + y])
        fail(`row ${y} is ${JSON.stringify(lines[y * 2])}, ` +
             `expected ${JSON.stringify(bw[at + 1 + y])}`);

// 2. It is in colour at all, and only where the fluid is.
const ink = lines.filter((_, i) => i % 2).join("");
const painted = [...ink].filter((c) => c !== "0").length;
if (painted < 100)
    fail(`only ${painted} cells carry a background`);

// 3. The whole frame, colours and all.
const golden = readFileSync(join(HERE, "colour.log"), "utf8");
if (got !== golden) {
    const a = got.split("\n"), b = golden.split("\n");
    for (let i = 0; i < Math.max(a.length, b.length); i++)
        if (a[i] !== b[i])
            fail(`line ${i + 1} is ${JSON.stringify(a[i])}, ` +
                 `expected ${JSON.stringify(b[i])}`);
    fail("the frames differ in length alone");
}

console.log(`asciifluid: frame ${AT} in colour, ${painted} cells painted`);
