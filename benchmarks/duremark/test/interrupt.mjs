// A ladder stopped by a signal: the workloads park nowhere, so the zero-length
// sleep between two steps is the only place one can be collected.
//
// `kill -INT` rather than a typed ^C, for the reason dhrystone's case gives:
// a computing foreground program leaves run() no window to type in. The clock
// is frozen here, so every step measures zero and the ladder never converges —
// which also exercises the guard against dividing by that zero.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));
const APPS = resolve(HERE, "../../..");
const CORE = resolve(APPS, "../braam-core");

const DEFAULTS = {
    kernel: join(CORE, "build/kernel.wasm"),
    rootfs: join(CORE, "build/web/rootfs.zip"),
    binary: join(APPS, "build/benchmarks/duremark/duremark.wasm"),
};

const opt = { ...DEFAULTS };
for (let i = 2; i < process.argv.length; i++) {
    const m = /^--(kernel|rootfs|binary)(?:=(.*))?$/.exec(process.argv[i]);
    if (!m) {
        console.error("usage: interrupt.mjs [--kernel=<wasm>] [--rootfs=<zip>] " +
                      "[--binary=<wasm>]");
        process.exit(2);
    }
    opt[m[1]] = m[2] !== undefined ? m[2] : process.argv[++i];
}

const die = (msg) => {
    console.error(`duremark: ${msg}`);
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
H.store.files.set("/bin/duremark", new Uint8Array(readFileSync(opt.binary)));

for (const line of ["duremark &", "kill -INT %1"]) {
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

// 1. It got as far as the ladder and then stopped on the signal.
if (!/Try \d+ iterations/.test(flat))
    die(`the ladder never started: ${shown()}`);
if (!flat.includes("Interrupted: the results below are the last pass that finished."))
    die(`the ladder was not interrupted: ${shown()}`);

// 2. Nothing divided by the zero every step measures here: an inf or a nan on
//    the report is that guard having failed.
for (const bad of ["inf", "nan"])
    if (flat.includes(bad))
        die(`the report divided by a zero total: ${shown()}`);

// 3. The status is 130, which the shell's job line names.
if (!flat.includes("interrupt duremark"))
    die(`the job did not end as an interrupt: ${shown()}`);

console.log(`duremark ok: the ladder stopped on a signal (${took} ms)`);
