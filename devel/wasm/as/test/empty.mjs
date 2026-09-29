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
    [["a.wat"], ["a.wat"], ["a.o"]],
    [["--module", "a.wat"], ["a.wat"], ["a.wasm"]],
    [["a.wat", "b.wat"], ["a.wat", "b.wat"], ["a.o", "b.o"]],
    [["-o", "x.bin", "a.wat"], ["a.wat"], ["x.bin"]],
    [["-oy", "a.wat"], ["a.wat"], ["y"]],
    [["noext"], ["noext"], ["noext.o"]],
    [["a.b.wat"], ["a.b.wat"], ["a.b.o"]],
    [[".wat"], [".wat"], [".wat.o"]],
    [["--", "-a.wat"], ["-a.wat"], ["-a.o"]],
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
    [[], [], ["no input file specified"]],
    [["--foo", "a.wat"], ["a.wat"], ["unknown argument: --foo"]],
    [["a.wat", "-o"], ["a.wat"], ["no value for -o"]],
    [["-o", "x", "a.wat", "b.wat"], ["a.wat", "b.wat"],
     ["multiple input files cannot be used in combination with -o"]],
    [["nosuch.wat", "a.wat"], ["a.wat"], ["cannot open nosuch.wat: not found"]],
    [["--module", "a.wasm"], ["a.wasm"], ["output would overwrite the input: a.wasm"]],
    [["-o", "a.wat", "a.wat"], ["a.wat"], ["output would overwrite the input: a.wat"]],
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
    const r = as.run(["nosuch.wat", "a.wat"], { "a.wat": "(module)" });
    if (!r.files["a.o"])
        bad.push("nosuch.wat a.wat: a.o not written");
}

{
    const r = as.run(["--help"]);
    if (r.status !== 0 || !r.out.startsWith("usage: as"))
        bad.push(`--help: status ${r.status}: ${r.out}${r.err}`);
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`as ok: ${RUNS.length} runs wrote the empty module or object, and ${ERRORS.length} errors`);
