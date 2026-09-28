// Shared by ld's cases: the fixture manifest, a booted kernel, and the
// oracle a linked fixture is held to — its module surface and its output.
// The oracle takes bytes, so wasm-ld's links and ld's are judged alike.

import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../../test/sdk.mjs";

export const HERE = dirname(fileURLToPath(import.meta.url));
export const APPS = resolve(HERE, "../../../..");
export const MANIFEST = join(APPS, "build/devel/wasm/ld/test/fixtures.json");
export const GOLDEN = join(HERE, "golden");

export function die(msg) {
    console.error("ld: " + msg);
    process.exit(1);
}

export function ok(msg) {
    console.log(msg ? "ld ok: " + msg : "ld ok");
}

// { ld, objdump, ar, sdk_libs, fixtures }, every path absolute.
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

// ---------------------------------------------------------------- two linkers

// What braam_add_program passes, less what the linker does not do yet.
export const FLAGS = ["--import-memory", "--initial-memory=1048576",
    "--no-entry", "--gc-sections", "--stack-first", "-z", "stack-size=131072"];

// ld on Braam and wasm-ld on the host, given the same inputs, each by its
// base name in one directory. Each returns { out, err, status, why }.
export function linkers(H, m) {
    const tmp = mkdtempSync(join(tmpdir(), "ld-"));
    process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
    plant(H, "/bin/ld", new Uint8Array(readFileSync(m.ld)));
    const where = new Map();

    function place(path) {
        const b = basename(path);
        if (where.has(b) && where.get(b) !== path)
            die(`two inputs are named ${b}`);
        if (!where.has(b)) {
            where.set(b, path);
            copyFileSync(path, join(tmp, b));
            plant(H, `/tmp/${b}`, new Uint8Array(readFileSync(path)));
        }
        return b;
    }

    function inputs(fx) {
        return [...fx.objects, ...(fx.archives ?? []), ...fx.libs].map(place);
    }

    function wasm_ld(args) {
        const r = spawnSync(m.wasm_ld, [...args, "-o", "out.wasm"], { cwd: tmp, encoding: "utf8" });
        if (r.error)
            die(`cannot run ${m.wasm_ld}: ${r.error.message}`);
        let why = "";
        try {
            why = readFileSync(join(tmp, "why"), "utf8");
        } catch {}
        rmSync(join(tmp, "why"), { force: true });
        return { out: r.stdout, err: r.stderr, status: r.status, why };
    }

    function ld(args) {
        for (const k of ["/tmp/o", "/tmp/e", "/tmp/s", "/tmp/why"])
            H.store.files.delete(k);
        plant(H, "/tmp/rsp", args.join("\n") + "\n");
        run(H, "cd /tmp; ld @rsp >o 2>e; echo $? >s");
        return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "",
                 status: Number(get(H, "/tmp/s")), why: get(H, "/tmp/why") ?? "" };
    }

    return { tmp, inputs, wasm_ld, ld };
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

// Names from the `name` section's subsection `sub` (1 functions, 7 globals),
// index -> name.
export function names_of(b, sub) {
    const sec = (sections(b) || []).find((s) => s.id === 0 && s.name === "name");
    const names = new Map();
    if (!sec)
        return names;
    const body = sec.body;
    for (let at = 0; at < body.length;) {
        const id = body[at];
        const [size, p] = leb(body, at + 1);
        if (id === sub) {
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

export function function_names(b) {
    return names_of(b, 1);
}

function sleb(b, at) {
    let v = 0, shift = 0, x;
    do {
        x = b[at++];
        v += (x & 0x7f) * 2 ** shift;
        shift += 7;
    } while (x & 0x80);
    if (x & 0x40)
        v -= 2 ** shift;
    return [v, at];
}

const VALTYPES = { 0x7f: "i32", 0x7e: "i64", 0x7d: "f32", 0x7c: "f64", 0x7b: "v128",
                   0x70: "funcref", 0x6f: "externref" };

// A constant expression's value when it is `i32.const v`, else "-"; and
// where it ends.
function expr(b, at) {
    let v = "-";
    if (b[at] === 0x41)
        [v, at] = sleb(b, at + 1);
    while (b[at] !== 0x0b)
        at++;
    return [v, at + 1];
}

// A linked module's index spaces, as `ld --dump-layout` prints them.
export function index_spaces(bytes) {
    const secs = sections(bytes);
    const body = (id) => secs.find((s) => s.id === id)?.body;
    const out = [];
    const vec = (b, at, each) => {
        let [n, p] = leb(b, at);
        for (let i = 0; i < n; i++)
            p = each(i, p);
        return p;
    };
    const types = body(1);
    if (types)
        vec(types, 0, (i, p) => {
            const list = [];
            p = vec(types, p + 1, (j, q) => (list.push(VALTYPES[types[q]]), q + 1));
            const res = [];
            p = vec(types, p, (j, q) => (res.push(VALTYPES[types[q]]), q + 1));
            out.push(`type (${list.join(", ")}) -> ${res[0] ?? "void"}`);
            return p;
        });
    const m = new WebAssembly.Module(bytes);
    for (const i of WebAssembly.Module.imports(m))
        out.push(`import ${i.module}.${i.name} ${i.kind}`);
    const imported = WebAssembly.Module.imports(m).filter((i) => i.kind === "global").length;
    const gnames = names_of(bytes, 7);
    const globals = body(6);
    if (globals)
        vec(globals, 0, (i, p) => {
            const type = VALTYPES[globals[p]];
            const mut = globals[p + 1] ? "mut" : "const";
            let v;
            [v, p] = expr(globals, p + 2);
            out.push(`global ${imported + i} ${type} ${mut} ${v} ${gnames.get(imported + i)}`);
            return p;
        });
    const table = body(4);
    if (table) {
        const [, at] = leb(table, 0);
        const flags = table[at + 1];
        const [min, p] = leb(table, at + 2);
        const [max] = flags & 1 ? leb(table, p) : ["-"];
        out.push(`table ${min} ${max}`);
    }
    const fnames = function_names(bytes);
    const elem = body(9);
    if (elem)
        vec(elem, 0, (i, p) => {
            if (elem[p] !== 0)
                die(`element segment of kind ${elem[p]}`);
            let base;
            [base, p] = expr(elem, p + 1);
            return vec(elem, p, (j, q) => {
                let f;
                [f, q] = leb(elem, q);
                out.push(`elem ${base + j} ${fnames.get(f)}`);
                return q;
            });
        });
    const code = body(10);
    const ctors = [...fnames].find(([, n]) => n === "__wasm_call_ctors");
    if (code && ctors) {
        const imported = WebAssembly.Module.imports(m).filter((i) => i.kind === "function").length;
        let [, p] = leb(code, 0);
        for (let i = imported; i < ctors[0]; i++) {
            const [size, q] = leb(code, p);
            p = q + size;
        }
        [, p] = leb(code, p);
        for (p++; code[p] === 0x10;) {
            let f;
            [f, p] = leb(code, p + 1);
            out.push(`ctor ${fnames.get(f)}`);
            while (code[p] === 0x1a)
                p++;
        }
    }
    return out;
}

// wasm-ld's -Map, as `ld --dump-layout` prints its memory map: GLOBAL,
// CODE and DATA, without file offsets, and without the chunks in `skip`.
export function map_layout(map, skip = []) {
    const out = [];
    let keep = false, skipping = false;
    for (const line of map.split("\n").slice(1)) {
        if (!line)
            continue;
        const vma = line.slice(0, 8), size = line.slice(18, 26), rest = line.slice(27);
        if (vma.trim() === "-" && !rest.startsWith(" ")) {
            keep = ["GLOBAL", "CODE", "DATA"].includes(rest);
            if (keep)
                out.push(rest);
            continue;
        }
        if (!keep)
            continue;
        if (!rest.startsWith(" "))
            skipping = false;
        else if (!rest.startsWith("                "))
            skipping = skip.includes(rest.trim());
        if (!skipping)
            out.push(`${vma} ${size} ${rest}`);
    }
    return out;
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
