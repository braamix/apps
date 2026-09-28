// Shared by wlink's cases: the fixture manifest, a booted kernel, and the
// oracle a linked fixture is held to — its module surface and its output.
// The oracle takes bytes, so wasm-ld's links and wlink's are judged alike.

import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, readFileSync } from "node:fs";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

export const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../..");
export const MANIFEST = join(APPS, "build/devel/wlink/test/fixtures.json");
export const GOLDEN = join(HERE, "golden");

export function die(msg) {
    console.error("wlink: " + msg);
    process.exit(1);
}

export function ok(msg) {
    console.log(msg ? "wlink ok: " + msg : "wlink ok");
}

// { wlink, objdump, ar, sdk_libs, fixtures }, every path absolute.
export function manifest() {
    if (!existsSync(MANIFEST))
        die(`no manifest at ${MANIFEST} — run make`);
    const m = JSON.parse(readFileSync(MANIFEST, "utf8"));
    delete m.fixtures[""];
    delete m.bad[""];
    return m;
}

// name -> { objects, archives, libs, reference }.
export function fixtures() {
    return manifest().fixtures;
}

export async function boot() {
    for (const [what, path] of [["kernel", KERNEL], ["rootfs", ROOTFS]])
        if (!existsSync(path))
            die(`no ${what} at ${path} — run \`make\``);
    const H = await import(HARNESS);
    await H.init(KERNEL, ROOTFS);
    H.kernel().init(0);
    if (H.run(0) !== -1)
        die("the shell did not park on the keyboard");
    H.regrid(80, 24, "resize returned no screen descriptor");
    if (!H.store.files.has("/bin/sh"))
        die("the archive did not unpack");
    return H;
}

export function plant(H, path, bytes) {
    const b = typeof bytes === "string" ? new TextEncoder().encode(bytes) : bytes;
    H.store.files.set(path, b);
}

export function get(H, path) {
    const b = H.store.files.get(path);
    return b ? new TextDecoder().decode(b) : null;
}

let now = 1;

// The key ring holds 64 and type() does not check.
export function run(H, cmd) {
    if (cmd.length > 60)
        die(`the command line is ${cmd.length} keys, and the ring holds 64`);
    H.submit(cmd, now++);
    if (H.run(now++) !== -1)
        die("the kernel did not settle after: " + cmd);
}

// Runs a linked program; its stdout and its exit status.
export function execute(H, bytes) {
    plant(H, "/bin/fx", bytes);
    H.store.files.delete("/tmp/o");
    run(H, "fx >/tmp/o; echo $? >/tmp/s");
    return { out: get(H, "/tmp/o"), status: Number(get(H, "/tmp/s")) };
}

// ---------------------------------------------------------------- reading wasm

function leb(b, at) {
    let v = 0, shift = 0, x;
    do {
        x = b[at++];
        v += (x & 0x7f) * 2 ** shift;
        shift += 7;
    } while (x & 0x80);
    return [v, at];
}

function name(b, at) {
    const [n, p] = leb(b, at);
    return [new TextDecoder().decode(b.subarray(p, p + n)), p + n];
}

// [{ id, name, body }], body being the contents after a custom section's name.
export function sections(b) {
    if (b.length < 8 || b[0] !== 0 || b[1] !== 0x61 || b[2] !== 0x73 || b[3] !== 0x6d)
        return null;
    if (b[4] !== 1 || b[5] || b[6] || b[7])
        return null;
    const out = [];
    for (let at = 8; at < b.length;) {
        const id = b[at];
        const [size, p] = leb(b, at + 1);
        let body = b.subarray(p, p + size), nm = "";
        if (id === 0) {
            const [s, q] = name(b, p);
            nm = s;
            body = b.subarray(q, p + size);
        }
        out.push({ id, name: nm, body });
        at = p + size;
    }
    return out;
}

// Function names from the `name` section, index -> name.
export function function_names(b) {
    const sec = (sections(b) || []).find((s) => s.id === 0 && s.name === "name");
    const names = new Map();
    if (!sec)
        return names;
    const body = sec.body;
    for (let at = 0; at < body.length;) {
        const id = body[at];
        const [size, p] = leb(body, at + 1);
        if (id === 1) {
            let [count, q] = leb(body, p);
            for (let i = 0; i < count; i++) {
                let idx, s;
                [idx, q] = leb(body, q);
                [s, q] = name(body, q);
                names.set(idx, s);
            }
        }
        at = p + size;
    }
    return names;
}

// A relocatable object: a module with a `linking` section of version 2.
export function check_object(path) {
    const s = sections(new Uint8Array(readFileSync(path)));
    if (!s)
        return `${path}: not a wasm module`;
    const linking = s.find((x) => x.id === 0 && x.name === "linking");
    if (!linking)
        return `${path}: no linking section`;
    if (linking.body[0] !== 2)
        return `${path}: linking version ${linking.body[0]}`;
    return null;
}

export function check_archive(path) {
    const b = readFileSync(path);
    return b.subarray(0, 8).toString("latin1") === "!<arch>\n" ? null : `${path}: not an ar archive`;
}

// ---------------------------------------------------------------- the oracle

const IMPORTS = ["env.memory", "kernel.sys", "kernel.sys_async"];
const EXPORTS = ["_alloc", "_free", "_resume", "_sig", "_start"];
const EXTRA_EXPORTS = { export: ["fx_extra"] };
const BRAAM_MAGIC = 0x6d617262;

// What any link of fixture `name` must look like. A list of complaints.
export function check_surface(name, bytes) {
    const bad = [];
    let m;
    try {
        m = new WebAssembly.Module(bytes);
    } catch (e) {
        return [`does not compile: ${e.message}`];
    }
    const imports = WebAssembly.Module.imports(m).map((i) => `${i.module}.${i.name}`).sort();
    if (imports.join() !== IMPORTS.join())
        bad.push(`imports ${imports.join(" ")}`);
    const want = [...EXPORTS, ...(EXTRA_EXPORTS[name] || [])].sort();
    const exports = WebAssembly.Module.exports(m).map((e) => e.name).sort();
    if (exports.join() !== want.join())
        bad.push(`exports ${exports.join(" ")}`);
    const stamp = WebAssembly.Module.customSections(m, "braam");
    if (stamp.length !== 1 || stamp[0].byteLength !== 20)
        bad.push("no braam section of five words");
    else if (new Uint32Array(stamp[0])[0] !== BRAAM_MAGIC)
        bad.push("braam section has the wrong magic");
    if (name === "export") {
        const fns = new Set(function_names(bytes).values());
        if (!fns.has("fx_kept"))
            bad.push("fx_kept was dropped");
        if (fns.has("fx_dropped"))
            bad.push("fx_dropped was kept");
    }
    return bad;
}

// Runs it and compares with golden/<name>.out.
export function check_run(H, name, bytes) {
    const want = readFileSync(join(GOLDEN, name + ".out"), "utf8");
    const { out, status } = execute(H, bytes);
    const bad = [];
    if (status !== 0)
        bad.push(`exit status ${status}`);
    if (out !== want)
        bad.push(`output ${JSON.stringify(out)}, expected ${JSON.stringify(want)}`);
    return bad;
}
