// A run cut short reports the stretches that finished — the point of making it
// in stretches at all.
//
// `kill -INT` rather than a typed ^C: run() pumps the kernel until it is idle,
// and a foreground program that computes without parking never is, so there is
// no window to type in. A background job with a second command line queued
// behind it gives the shell its turn between two stretches.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/benchmarks/dhrystone/dhrystone.wasm"),
    // Long enough that the interrupt lands in the middle of it, short enough
    // that the whole case is a few seconds.
    runs: "9000000",
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary|runs)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: interrupt.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>] [--runs=<n>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`dhrystone: ${msg}`);
    process.exit(1);
};

for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
    if (!existsSync(path))
        die(`no ${what} at ${path} — build ../braam-core first`);
if (!existsSync(opt.binary))
    die(`no binary at ${opt.binary} — run make`);

const H = await import(join(CORE, "test/system/harness.mjs"));

// ---------------------------------------------------------------- the run

await H.init(opt.kernel, opt.rootfs);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(80, 24, "resize returned no screen descriptor");
H.store.files.set("/bin/dhrystone", new Uint8Array(readFileSync(opt.binary)));

// Both lines before the first tick, so the second is read while the first runs.
for (const line of [`dhrystone ${opt.runs} &`, "kill -INT %1"]) {
    if (line.length > 60)
        die(`the command line is ${line.length} keys, and the ring holds 64`);
    H.type(line);
    H.press(H.KEY.ENTER);
}

const started = Date.now();
if (H.run(100) !== -1)
    die("the benchmark left the kernel with work to do");
const took = Date.now() - started;
const s = H.screen();
const flat = H.rows(s).join(" ").replace(/\s+/g, " ");
const shown = () => JSON.stringify(H.rows(s).filter((l) => l.trim()));

// ------------------------------------------------------------- assertions

// 1. It stopped early, and said how far it had got.
const cut = /Interrupted after (\d+) runs/.exec(flat);
if (!cut)
    die(`the run was not interrupted: ${shown()}`);
const done = Number(cut[1]);
if (done <= 0 || done >= Number(opt.runs))
    die(`interrupted after ${done} of ${opt.runs} runs, which is not in the middle`);

// 2. The report is still the report: an interrupt is a shorter run, not a
//    different program. Its head has scrolled off 24 rows, and the clock is
//    frozen here, so only the tail can be checked.
for (const want of ["Str_1_Loc: DHRYSTONE PROGRAM, 1'ST STRING",
                    "Str_2_Loc: DHRYSTONE PROGRAM, 2'ND STRING",
                    "Measured time too small"])
    if (!flat.includes(want))
        die(`the report is missing ${JSON.stringify(want)}: ${shown()}`);

// 3. The status is 130, which the shell's job line names.
if (!flat.includes("interrupt dhrystone"))
    die(`the job did not end as an interrupt: ${shown()}`);

console.log(`dhrystone ok: interrupted after ${done} of ${opt.runs} runs (${took} ms)`);
