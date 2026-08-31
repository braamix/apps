// Boots BESM-6 Unix under braam-core's system harness, headless, on one screen
// and then on two.
//
// The transcript is not asserted byte for byte: the guest is a real Unix and
// what it prints depends on its own filesystem's dates and free lists. What is
// asserted is what the run *meant* -- the kernel sized memory, the root pack
// mounted, a command came off it, /etc/rc fscked /usr through the drums, and a
// getty reached each Consul line.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/emulators/simbesm/besm6.wasm"),
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: boot.mjs [--kernel=<wasm>] [--rootfs=<zip>] [--binary=<wasm>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`besm6: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

// After the paths are checked: the harness exits the process itself, and would
// not say what to build.
const H = await import(join(CORE, "test/system/harness.mjs"));

// ---------------------------------------------------------------- the run

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
// A screen for a program rather than a shell, which is what a page saying
// `shell: false' asks for: without it the shell there holds the raw keys and
// the second Consul line has nowhere to be typed at.
H.regrid(80, 24, "resize returned no second screen", 1, H.TERM_NO_SHELL);
if (!H.store.files.has("/bin/sh"))
    die("the archive did not unpack");

// Planted, not packed: exec takes any stamped path, and /bin is ordinary store
// content once boot has unpacked the archive.
H.store.files.set("/bin/besm6", new Uint8Array(readFileSync(opt.binary)));
for (const f of ["unix", "root3072.disk", "usr3100.disk"])
    H.store.files.set("/data/" + f, new Uint8Array(readFileSync(join(HERE, "../data", f))));

let now = 100;
const shown = (term) => H.rows(H.screen(term)).join("\n");

// The harness pumps until the kernel is idle, and the machine parks on its
// pacing timer once a burst -- so a step is a burst, not a wall-clock moment.
function want(text, why, { term = 0, steps = 800 } = {}) {
    for (let i = 0; i < steps; i++) {
        H.run((now += 20));
        if (shown(term).includes(text))
            return;
    }
    for (const t of [0, 1])
        console.error(`--- screen ${t} ---\n` +
                      H.rows(H.screen(t)).filter((l) => l.trim()).join("\n"));
    die(`timed out waiting for ${why}`);
}

const type = (text, term = 0) => {
    // The key ring holds 32 and the harness does not check.
    if (text.length > 30)
        die(`"${text}" is ${text.length} keys, and the ring holds 32`);
    H.type(text, term);
    H.press(H.KEY.ENTER, 0, term);
};

type("BESM6_PREFIX=/data besm6");

// Attaching a console says so only under `set tty debug'; that both lines came
// up is asserted below, where a getty reaches each of them.

// main() sized memory and mounted the root, so the kernel loaded and md00 is
// attached -- without it the boot panics in iinit().
want("phys mem  = 3072 kbytes", "startup()");
want("swap size = 3072 kbytes", "startup()");
want("root size = 6000 kbytes", "iinit() -- is md00 attached?");
want("Single-user mode", "the single-user banner");

// A command read off the root pack and typed through tty25 in raw8.
type("ls /bin");
want("sync", "ls /bin");

// The machine has no clock-calendar; the operator types the date.
type("date 2608301200");
want("Aug 30 12:00:00", "date");

// ^D ends the shell: init reaps it, runs /etc/rc through a second shell, which
// fscks and mounts /dev/rmd1 on /usr.  That exercises md01 and the drums --
// exece() stages its argument list in swap, so with no drum every exec is EIO.
H.press("d".codePointAt(0), H.CTRL);
want("Going multi-user", "init to run /etc/rc");
want("/dev/rmd1", "fsck of the /usr pack -- is md01 attached?");
want("Phase 5 - Check Free List", "fsck to finish", { steps: 3000 });
want("login:", "a getty on the console");

// The other half of the second screen: /etc/rc puts a getty on every line, and
// tty26's reaches the grid the program opened rather than its own.
want("login:", "a getty on the second Consul line", { term: 1, steps: 3000 });

// Keys follow the screen, not the process.
for (const ch of "zxqw")
    H.press(ch.codePointAt(0), 0, 1);
want("zxqw", "what was typed on the second screen", { term: 1 });
if (shown(0).includes("zxqw"))
    die("what was typed on the second screen reached the first as well");

// The packs are written in place, so the program copies them out of the store
// on its first run and the drums are created beside them.
for (const [f, size] of [["root3072.disk", 8256000], ["usr3100.disk", 8256000],
                         ["unix0.drum", 4160], ["unix1.drum", 0]]) {
    const got = H.store.files.get("/home/.besm6/" + f);
    if (!got)
        die(`/home/.besm6/${f} was not staged`);
    if (got.length !== size)
        die(`/home/.besm6/${f} is ${got.length} bytes, expected ${size}`);
}

console.log("besm6 ok: Unix booted multi-user on two screens");
