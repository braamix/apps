// Shared boot for the c4 cases: plant the binary, drive a command whose
// stdout is a file, read that file back. A long listing does not fit on 24
// rows, and c4 reading a file is the interesting path anyway.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

export const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const SHARE = join(APPS, "devel/c4/share");

export const DEFAULTS = {
    kernel: KERNEL,
    rootfs: ROOTFS,
    binary: join(APPS, "build/devel/c4/c4.wasm"),
};

export function parse_opt(argv, keys) {
    const opt = { ...DEFAULTS };
    for (let i = 2; i < argv.length; i++) {
        const m = /^--([^=]+)(?:=(.*))?$/.exec(argv[i]);
        if (!m || !keys.includes(m[1]) && !(m[1] in DEFAULTS)) {
            console.error("usage: [--kernel=<wasm>] [--rootfs=<zip>] [--binary=<wasm>]");
            process.exit(2);
        }
        opt[m[1]] = m[2] !== undefined ? m[2] : argv[++i];
    }
    return opt;
}

export function die(msg) {
    console.error("c4: " + msg);
    process.exit(1);
}

export async function boot(opt) {
    for (const [what, path] of [["kernel", opt.kernel], ["rootfs", opt.rootfs]])
        if (!existsSync(path))
            die(`no ${what} at ${path} — run \`make\``);
    if (!existsSync(opt.binary))
        die("no binary at " + opt.binary + " — run make");

    const H = await import(HARNESS);
    await H.init(opt.kernel, opt.rootfs);
    H.kernel().init(0);
    if (H.run(0) !== -1)
        die("the shell did not park on the keyboard");
    H.regrid(80, 24, "resize returned no screen descriptor");
    if (!H.store.files.has("/bin/sh"))
        die("the archive did not unpack");
    H.store.files.set("/bin/c4", new Uint8Array(readFileSync(opt.binary)));
    return H;
}

export function plant(H, path, bytes) {
    const b = typeof bytes === "string" ? new TextEncoder().encode(bytes) : bytes;
    H.store.files.set(path, b);
}

export function plant_file(H, dest, src) {
    plant(H, dest, readFileSync(src));
}

let now = 1;

export function run(H, cmd) {
    if (cmd.length > 60)
        die(`the command line is ${cmd.length} keys, and the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die("the kernel did not settle after: " + cmd);
}

export function get(H, path) {
    const b = H.store.files.get(path);
    return b ? new TextDecoder().decode(b) : "";
}

export function ok(msg) {
    console.log(msg ? "c4 ok: " + msg : "c4 ok");
}
