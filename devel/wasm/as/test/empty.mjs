// as's command line: an empty source is llvm-mc's empty object, which V8
// must load; where each output goes, and what is refused.

import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../../wat/host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const BUILD = join(HERE, "../../../../build/devel/wasm");
const AS_WASM = join(BUILD, "as/as.wasm");
const WAT_WASM = join(BUILD, "wat/wat.wasm");
// The memory imported, a linking section, version 2, and no symbols; every
// size a padded LEB.
const OBJECT = [
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x02, 0x98, 0x80, 0x80, 0x80, 0x00, 0x01,
    0x03, ...Buffer.from("env"), 0x0f, ...Buffer.from("__linear_memory"), 0x02, 0x00, 0x00,
    0x00, 0x89, 0x80, 0x80, 0x80, 0x00, 0x07, ...Buffer.from("linking"), 0x02,
];

function die(msg) {
    console.error("as: " + msg);
    process.exit(1);
}

for (const f of [AS_WASM, WAT_WASM])
    if (!existsSync(f))
        die(`no ${f} — run make`);
const as = await assembler(WAT_WASM, { as: AS_WASM });
const run = (args, inputs) => as.run(args, inputs, null, "as");
const bad = [];
const hex = (b) => [...b].map((x) => x.toString(16).padStart(2, "0")).join(" ");

// [arguments, inputs, the outputs expected]
const RUNS = [
    [["a.s"], ["a.s"], ["a.o"]],
    [["a.s", "b.s"], ["a.s", "b.s"], ["a.o", "b.o"]],
    [["-o", "x.bin", "a.s"], ["a.s"], ["x.bin"]],
    [["-oy", "a.s"], ["a.s"], ["y"]],
    [["noext"], ["noext"], ["noext.o"]],
    [["a.b.s"], ["a.b.s"], ["a.b.o"]],
    [[".s"], [".s"], [".s.o"]],
    [["--", "-a.s"], ["-a.s"], ["-a.o"]],
];
for (const [args, names, want] of RUNS) {
    const r = run(args, Object.fromEntries(names.map((n) => [n, "# nothing\n"])));
    const got = Object.keys(r.files).sort();
    if (r.status !== 0 || r.err || r.out)
        bad.push(`${args.join(" ")}: status ${r.status}: ${r.out}${r.err}`);
    else if (got.join(" ") !== want.join(" "))
        bad.push(`${args.join(" ")}: wrote ${got.join(" ")}, expected ${want.join(" ")}`);
    for (const [name, b] of Object.entries(r.files)) {
        if (hex(b) !== hex(OBJECT))
            bad.push(`${args.join(" ")}: ${name} is ${hex(b)}`);
        try {
            new WebAssembly.Module(b);
        } catch (e) {
            bad.push(`${args.join(" ")}: V8 refuses ${name}: ${e.message}`);
        }
    }
}

// [arguments, inputs, the messages]
const ERRORS = [
    [["-o", "a.o"], [], ["no input file specified"]],
    [["--foo", "a.s"], ["a.s"], ["unknown argument: --foo"]],
    [["a.s", "-o"], ["a.s"], ["no value for -o"]],
    [["-o", "x", "a.s", "b.s"], ["a.s", "b.s"],
     ["multiple input files cannot be used in combination with -o"]],
    [["nosuch.s", "a.s"], ["a.s"], ["cannot open nosuch.s: not found"]],
    [["a.o"], ["a.o"], ["output would overwrite the input: a.o"]],
    [["-o", "a.s", "a.s"], ["a.s"], ["output would overwrite the input: a.s"]],
];
for (const [args, names, msgs] of ERRORS) {
    const r = run(args, Object.fromEntries(names.map((n) => [n, "\n"])));
    const err = msgs.map((s) => `as: error: ${s}\n`).join("");
    if (r.status !== 1 || r.err !== err)
        bad.push(`${args.join(" ")}: status ${r.status}: ${JSON.stringify(r.err)}, ` +
                 `expected ${JSON.stringify(err)}`);
}
// The next file is still assembled after one that fails.
{
    const r = run(["nosuch.s", "a.s"], { "a.s": "\n" });
    if (!r.files["a.o"])
        bad.push("nosuch.s a.s: a.o not written");
}

// The usage, bare or asked for.
for (const args of [[], ["-h"], ["--help"]]) {
    const r = run(args);
    if (r.status !== 0 || r.err || !r.out.startsWith("Usage:\n    as "))
        bad.push(`as ${args.join(" ")}: status ${r.status}: ${r.out}${r.err}`);
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`as ok: ${RUNS.length} runs wrote the empty object, and ${ERRORS.length} errors`);
