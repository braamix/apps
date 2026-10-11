// as against llvm-mc: every probe/*.s and corpus/*.s assembled by as on
// Braam and by clang on the host, and the two objects equal byte for byte,
// with the same warnings. Then the one place as differs on purpose: a
// plain select, which llvm writes as an invalid typed one.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { manifest } from "../../ld/test/wasmlib.mjs";
import { assembler } from "../../wat/host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const BUILD = join(HERE, "../../../../build/devel/wasm");
const AS_WASM = join(BUILD, "as/as.wasm");
const WAT_WASM = join(BUILD, "wat/wat.wasm");
const LLVM = dirname(manifest().objdump);
const CLANG = join(LLVM, "clang");

function die(msg) {
    console.error("objects: " + msg);
    process.exit(1);
}

for (const f of [AS_WASM, WAT_WASM, CLANG])
    if (!existsSync(f))
        die(`no ${f}`);
const as = await assembler(WAT_WASM, { as: AS_WASM });
const tmp = mkdtempSync(join(tmpdir(), "as-objects-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const bad = [];

// llvm's object for `name`, and what it said.
function llvm(name, text) {
    writeFileSync(join(tmp, name), text);
    const r = spawnSync(CLANG, ["--target=wasm32", "--no-default-config", "-c", name, "-o", "ref.o"],
                        { cwd: tmp, encoding: "utf8" });
    if (r.status !== 0)
        die(`clang refuses ${name}:\n${r.stderr}`);
    return { bytes: readFileSync(join(tmp, "ref.o")), err: r.stderr };
}

function objdump(bytes, args) {
    writeFileSync(join(tmp, "x.o"), bytes);
    return spawnSync(join(LLVM, "llvm-objdump"), [...args, "x.o"], { cwd: tmp, encoding: "utf8" }).stdout;
}

const sources = [];
for (const dir of ["probe", "corpus"])
    for (const f of readdirSync(join(HERE, dir)).sort())
        if (f.endsWith(".s"))
            sources.push([dir, f]);

let bytes = 0;
for (const [dir, f] of sources) {
    const text = readFileSync(join(HERE, dir, f));
    const want = llvm(f, text);
    const r = as.run([f], { [f]: text }, null, "as");
    const out = r.files[f.replace(/\.s$/, ".o")];
    const where = `${dir}/${f}`;
    if (r.status !== 0 || !out) {
        bad.push(`${where}: status ${r.status}\n${r.err}`);
        continue;
    }
    if (r.err !== want.err)
        bad.push(`${where}: as says\n${r.err}clang says\n${want.err}`);
    const got = Buffer.from(out);
    if (!got.equals(want.bytes)) {
        let k = 0;
        while (k < got.length && got[k] === want.bytes[k])
            k++;
        bad.push(`${where}: differs from byte ${k} (${got.length} bytes, llvm's ${want.bytes.length})\n` +
                 `as:\n${objdump(got, ["-h"])}llvm:\n${objdump(want.bytes, ["-h"])}`);
        continue;
    }
    try {
        new WebAssembly.Module(got);
    } catch (e) {
        bad.push(`${where}: V8 refuses it: ${e.message}`);
    }
    bytes += got.length;
}

// A plain select is 0x1b; llvm writes 0x1c 0x00, which no engine loads.
const SELECT = `\t.section\t.text.pick,"",@
\t.globl\tpick
\t.type\tpick,@function
pick:
\t.functype\tpick (i32, i32, i32) -> (i32)
\tlocal.get\t0
\tlocal.get\t1
\tlocal.get\t2
\tselect
\tend_function
`;
{
    const r = as.run(["pick.s"], { "pick.s": SELECT }, null, "as");
    const out = r.files["pick.o"];
    if (r.status !== 0 || !out) {
        bad.push(`select: status ${r.status}\n${r.err}`);
    } else {
        // llvm-objdump names 0x1b f32.select.
        if (!/: 1b\s+\t\S*select\n/.test(objdump(Buffer.from(out), ["-d"])))
            bad.push(`select: not the plain one:\n${objdump(Buffer.from(out), ["-d"])}`);
        try {
            new WebAssembly.Module(out);
        } catch (e) {
            bad.push(`select: V8 refuses it: ${e.message}`);
        }
    }
}

if (bad.length) {
    console.error(bad.slice(0, 10).join("\n"));
    die(`${bad.length} of ${sources.length + 1} differ`);
}
console.log(`objects ok: ${sources.length} sources, ${bytes} bytes as llvm-mc writes them; a plain select`);
