// What as refuses, and how it says so: each source below assembled by as on
// Braam and by clang on the host, both must refuse it, and the messages
// must be the same, line, column, the source line and the caret. llvm's
// fatal errors come without a place; as says them as `as: error: ...`.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { manifest } from "../../ld/test/wasmlib.mjs";
import { assembler } from "../../wat/host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const BUILD = join(HERE, "../../../../build/devel/wasm");
const AS_WASM = join(BUILD, "as/as.wasm");
const WAT_WASM = join(BUILD, "wat/wat.wasm");
const CLANG = join(dirname(manifest().objdump), "clang");

function die(msg) {
    console.error("errors: " + msg);
    process.exit(1);
}

for (const f of [AS_WASM, WAT_WASM, CLANG])
    if (!existsSync(f))
        die(`no ${f}`);
const as = await assembler(WAT_WASM, { as: AS_WASM });
const tmp = mkdtempSync(join(tmpdir(), "as-errors-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

// A function around `body`, each line a statement.
const fn = (body) => ["\t.section\t.text.f,\"\",@", "\t.type\tf,@function", "f:",
                      "\t.functype\tf () -> ()", ...body, "\tend_function"].join("\n") + "\n";
const lines = (...l) => l.join("\n") + "\n";

const CASES = [
    ["mnemonic", fn(["\tfoo.bar"])],
    ["no select types", fn(["\tselect\ti33"])],
    ["int8 list", lines("\t.section\t.data.x,\"\",@", "x:", "\t.int8\t1, 2", "\t.size\tx, 1")],
    ["int8 range", lines("\t.section\t.data.x,\"\",@", "x:", "\t.int8\t300", "\t.size\tx, 1")],
    ["int16 range", lines("\t.section\t.data.x,\"\",@", "x:", "\t.int16\t-32769", "\t.size\tx, 2")],
    ["nan payload", fn(["\tf64.const\tnan:0x1"])],
    ["inexact real", fn(["\tf32.const\t0.1"])],
    ["integer float", fn(["\tf32.const\t1"])],
    ["directive", lines("\t.frobnicate\tx")],
    ["no functype", fn(["\tcall\tg"])],
    ["no globaltype", fn(["\tglobal.get\tx", "\tdrop"])],
    ["global by index", fn(["\tglobal.get\t0", "\tdrop"])],
    ["redefinition", lines("\t.section\t.data.x,\"\",@", "x:", "x:", "\t.int8\t0", "\t.size\tx, 1")],
    ["no start", fn(["\tend_block"])],
    ["mismatch", fn(["\tblock", "\tend_loop"])],
    ["else alone", fn(["\telse"])],
    ["unmatched", fn(["\tblock"])],
    ["unended", lines("\t.section\t.text.f,\"\",@", "\t.type\tf,@function", "f:",
                      "\t.functype\tf () -> ()", "\t.section\t.text.g,\"\",@", "\t.type\tg,@function",
                      "g:", "\t.functype\tg () -> ()", "\tend_function")],
    ["local late", fn(["\tnop", "\t.local\ti32"])],
    ["local type", lines("\t.section\t.text.f,\"\",@", "\t.type\tf,@function", "f:",
                         "\t.functype\tf () -> ()", "\t.local\ti32, i33", "\tend_function")],
    ["data in code", fn(["\t.int32\t1"])],
    ["section flag", lines("\t.section\t.data.x,\"q\",@")],
    ["section flags changed", lines("\t.section\t.data.x,\"S\",@", "\t.section\t.data.x,\"\",@")],
    ["section no at", lines("\t.section\t.data.x,\"\"")],
    ["passive code", lines("\t.section\t.text.x,\"p\",@")],
    ["symbol type", lines("\t.type\tx,@banana")],
    ["type no label", lines("\t.type\t1,@function")],
    ["type no at", lines("\t.type\tx,function")],
    ["globaltype", lines("\t.globaltype\tg, i33")],
    ["globaltype modifier", lines("\t.globaltype\tg, i32, mutable")],
    ["tabletype", lines("\t.tabletype\tt, funky")],
    ["tabletype limit", lines("\t.tabletype\tt, funcref, x")],
    ["functype paren", lines("\t.functype\tg (i32 -> (i32)")],
    ["functype arrow", lines("\t.functype\tg (i32) (i32)")],
    ["functype type", lines("\t.functype\tg (i33) -> ()")],
    ["functype eol", lines("\t.functype\tg () -> () x")],
    ["export comma", lines("\t.export_name\tg g")],
    ["import name", lines("\t.import_name\tg, 5")],
    ["p2align word", fn(["\ti32.const\t0", "\ti32.load\t0:p3align=2", "\tdrop"])],
    ["p2align equal", fn(["\ti32.const\t0", "\ti32.load\t0:p2align 2", "\tdrop"])],
    ["p2align value", fn(["\ti32.const\t0", "\ti32.load\t0:p2align=x", "\tdrop"])],
    ["block type", fn(["\tblock\ti33", "\tend_block"])],
    ["too few", fn(["\tlocal.get"])],
    ["too many", fn(["\ti32.const\t1, 2"])],
    ["operand", fn(["\ti32.add\t1"])],
    ["operand token", fn(["\ti32.const\t@"])],
    ["comma", fn(["\tbr_table\t{0 1}"])],
    ["minus", fn(["\ti32.const\t-x"])],
    ["string", lines("\t.section\t.data.x,\"\",@", "x:", "\t.ascii\t\"abc", "\t.size\tx, 3")],
    ["escape", lines("\t.section\t.data.x,\"\",@", "x:", "\t.ascii\t\"a\\qb\"", "\t.size\tx, 3")],
    ["ascii no string", lines("\t.section\t.data.x,\"\",@", "x:", "\t.ascii\t5", "\t.size\tx, 1")],
    ["statement", lines("\t, x")],
    ["hex float", fn(["\tf64.const\t0x1.8"])],
    ["octal", lines("\t.section\t.data.x,\"\",@", "x:", "\t.int8\t09", "\t.size\tx, 1")],
    ["align", lines("\t.section\t.data.x,\"\",@", "\t.p2align\t40")],
    ["balign", lines("\t.section\t.data.x,\"\",@", "\t.balign\t3")],
    ["label in code", lines("\t.text", "\t.type\td,@object", "d:")],
    ["no size", lines("\t.section\t.data.x,\"\",@", "x:", "\t.int8\t0", "\t.section\t.text.f,\"\",@",
                      "\t.type\tf,@function", "f:", "\t.functype\tf () -> (i32)", "\ti32.const\tx",
                      "\tend_function")],
];

const bad = [];
for (const [what, text] of CASES) {
    const name = `${what.replace(/ /g, "-")}.s`;
    writeFileSync(join(tmp, name), text);
    const c = spawnSync(CLANG, ["--target=wasm32", "--no-default-config", "-c", name, "-o", "x.o"],
                        { cwd: tmp, encoding: "utf8" });
    const r = as.run([name], { [name]: text }, null, "as");
    if (c.status === 0) {
        bad.push(`${what}: clang takes it`);
        continue;
    }
    if (r.status === 0) {
        bad.push(`${what}: as takes it; clang says\n${c.stderr}`);
        continue;
    }
    // A fatal error is a backend's, said once and without a place.
    const fatal = /fatal error: error in backend: (.*)/.exec(c.stderr);
    const want = fatal ? `as: error: ${fatal[1]}\n` : c.stderr;
    if (r.err !== want)
        bad.push(`${what}:\nas says\n${r.err}clang says\n${want}`);
}
if (bad.length) {
    console.error(bad.join("\n"));
    die(`${bad.length} of ${CASES.length} differ`);
}
console.log(`errors ok: ${CASES.length} sources refused as llvm-mc refuses them`);
