// as's command line: (module) is the empty module, or with no --module the
// empty object, which V8 must load; where each output goes, and what is
// refused.

import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");
const EMPTY = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
// A linking section, version 2, and no symbols.
const OBJECT = [...EMPTY, 0x00, 0x09, 0x07, ...Buffer.from("linking"), 0x02];

function die(msg) {
    console.error("as: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
const as = await assembler(AS);
const bad = [];
const hex = (b) => [...b].map((x) => x.toString(16).padStart(2, "0")).join(" ");

// [arguments, inputs, the outputs expected]
const RUNS = [
    [["a.s"], ["a.s"], ["a.o"]],
    [["--module", "a.s"], ["a.s"], ["a.wasm"]],
    [["a.s", "b.s"], ["a.s", "b.s"], ["a.o", "b.o"]],
    [["-o", "x.bin", "a.s"], ["a.s"], ["x.bin"]],
    [["-oy", "a.s"], ["a.s"], ["y"]],
    [["noext"], ["noext"], ["noext.o"]],
    [["a.b.s"], ["a.b.s"], ["a.b.o"]],
    [[".s"], [".s"], [".s.o"]],
    [["--", "-a.s"], ["-a.s"], ["-a.o"]],
];
for (const [args, names, want] of RUNS) {
    const r = as.run(args, Object.fromEntries(names.map((n) => [n, "(module)\n"])));
    const got = Object.keys(r.files).sort();
    if (r.status !== 0 || r.err || r.out)
        bad.push(`${args.join(" ")}: status ${r.status}: ${r.out}${r.err}`);
    else if (got.join(" ") !== want.join(" "))
        bad.push(`${args.join(" ")}: wrote ${got.join(" ")}, expected ${want.join(" ")}`);
    for (const [name, b] of Object.entries(r.files)) {
        if (hex(b) !== hex(args.includes("--module") ? EMPTY : OBJECT))
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
    [["--module"], [], ["no input file specified"]],
    [["--foo", "a.s"], ["a.s"], ["unknown argument: --foo"]],
    [["a.s", "-o"], ["a.s"], ["no value for -o"]],
    [["-o", "x", "a.s", "b.s"], ["a.s", "b.s"],
     ["multiple input files cannot be used in combination with -o"]],
    [["nosuch.s", "a.s"], ["a.s"], ["cannot open nosuch.s: not found"]],
    [["--module", "a.wasm"], ["a.wasm"], ["output would overwrite the input: a.wasm"]],
    [["-o", "a.s", "a.s"], ["a.s"], ["output would overwrite the input: a.s"]],
];
for (const [args, names, msgs] of ERRORS) {
    const r = as.run(args, Object.fromEntries(names.map((n) => [n, "(module)\n"])));
    const err = msgs.map((s) => `as: error: ${s}\n`).join("");
    if (r.status !== 1 || r.err !== err)
        bad.push(`${args.join(" ")}: status ${r.status}: ${JSON.stringify(r.err)}, ` +
                 `expected ${JSON.stringify(err)}`);
}
// The next file is still assembled after one that fails.
{
    const r = as.run(["nosuch.s", "a.s"], { "a.s": "(module)" });
    if (!r.files["a.o"])
        bad.push("nosuch.s a.s: a.o not written");
}

// The usage, bare or asked for.
for (const args of [[], ["-h"], ["--help"]]) {
    const r = as.run(args);
    if (r.status !== 0 || r.err || !r.out.startsWith("Usage:\n    as "))
        bad.push(`as ${args.join(" ")}: status ${r.status}: ${r.out}${r.err}`);
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`as ok: ${RUNS.length} runs wrote the empty module or object, and ${ERRORS.length} errors`);
