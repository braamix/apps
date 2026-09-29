// size, held to llvm-size byte for byte: every fixture's objects, archives
// and reference program, the SDK's libraries and ld.wasm, in each format and
// radix, one command line per mode. Then the errors: what llvm-size prints
// beside them must still match, and the message is size's own.

import { execFileSync, spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync }
    from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { boot, get, manifest, plant, run } from "../../ld/test/wasmlib.mjs";

function die(msg) {
    console.error("size: " + msg);
    process.exit(1);
}

const m = manifest();
const SIZE = join(dirname(m.ld), "../size/size.wasm");
const LLVM_SIZE = join(dirname(m.objdump), "llvm-size");
const tmp = mkdtempSync(join(tmpdir(), "size-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

const H = await boot();
plant(H, "/bin/size", new Uint8Array(readFileSync(SIZE)));
const bad = [];

// Every input in one directory, on the host and in /tmp/w alike, so a
// relative name reads the same in both tools' output.
const dir = join(tmp, "w");
mkdirSync(dir);
H.store.dirs.add("/tmp/w");
function add(name, bytes) {
    writeFileSync(join(dir, name), bytes);
    plant(H, "/tmp/w/" + name, new Uint8Array(bytes));
    return name;
}

// A command line of any length, through a script, in /tmp/w; stdout,
// stderr, status.
function sh(cmd, stdin) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
        H.store.files.delete(k);
    plant(H, "/tmp/c", `cd /tmp/w; ${cmd} >/tmp/o 2>/tmp/e; echo $? >/tmp/s\n`);
    run(H, "sh /tmp/c");
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "", status: Number(get(H, "/tmp/s")) };
}

function llvm(args, input) {
    const r = spawnSync(LLVM_SIZE, args, { cwd: dir, encoding: "utf8", input });
    if (r.error)
        die(`llvm-size ${args.join(" ")}: ${r.error.message}`);
    return { out: r.stdout, status: r.status };
}

// The first line where two outputs part.
function differ(a, b) {
    const x = a.split("\n"), y = b.split("\n");
    for (let i = 0; i < Math.max(x.length, y.length); i++)
        if (x[i] !== y[i])
            return `line ${i + 1}: ${JSON.stringify(x[i])}, llvm-size ${JSON.stringify(y[i])}`;
    return "";
}

// ------------------------------------------------------------ llvm-size's output

const inputs = [add("ld.wasm", readFileSync(m.ld))];
const custom = (name) => {
    const n = new TextEncoder().encode(name);
    return [0, n.length + 3, n.length, ...n, 1, 2];
};
inputs.push(add("empty.wasm", new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0])));
inputs.push(add("custom.wasm", new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
                                               ...custom("a_long_custom_section_name")])));
let n = 0;
const seen = new Set();
for (const [name, fx] of Object.entries(m.fixtures)) {
    inputs.push(add(`fx_${name}.wasm`, readFileSync(fx.reference)));
    for (const o of fx.objects)
        if (!seen.has(o)) {
            seen.add(o);
            inputs.push(add(`o${n++}_${basename(o)}`, readFileSync(o)));
        }
    for (const a of fx.archives)
        if (!seen.has(a)) {
            seen.add(a);
            inputs.push(add(basename(a), readFileSync(a)));
        }
}
for (const f of readdirSync(m.sdk_libs).filter((n) => n.endsWith(".a")).sort())
    inputs.push(add(f, readFileSync(join(m.sdk_libs, f))));

const MODES = [[], ["-A"], ["-x"], ["-o"], ["-A", "-x"], ["-A", "-o"], ["-m"], ["-t"],
               ["-t", "-x"], ["-t", "-o"], ["-m", "-t"], ["-A", "-t"], ["-Bd"], ["-Ax"],
               ["--format=sysv", "--radix=16"], ["--format", "darwin", "--radix", "8"],
               ["--totals", "--common"]];
for (const mode of MODES) {
    const want = llvm([...mode, ...inputs]);
    if (want.status !== 0)
        die(`llvm-size ${mode.join(" ")}: status ${want.status}`);
    const got = sh(`size ${mode.join(" ")} ${inputs.join(" ")}`);
    if (got.status !== 0 || got.err)
        bad.push(`[${mode.join(" ")}]: status ${got.status}: ${got.err}`);
    else if (got.out !== want.out)
        bad.push(`[${mode.join(" ")}]: ${differ(got.out, want.out)}`);
}

// Standard input.
{
    const prog = readFileSync(m.fixtures.hello.reference);
    plant(H, "/tmp/in", new Uint8Array(prog));
    for (const mode of [[], ["-A"]]) {
        const want = llvm([...mode, "-"], prog);
        const got = sh(`size ${mode.join(" ")} - </tmp/in`);
        if (got.status !== 0 || got.out !== want.out)
            bad.push(`stdin [${mode.join(" ")}]: ${got.status} ${got.err} ${differ(got.out, want.out)}`);
    }
}

// ------------------------------------------------------------ errors

// An archive with a malformed member, a text member and an object: the
// object is sized, the text skipped, the malformed one an error.
const ar_dir = join(tmp, "ar");
mkdirSync(ar_dir);
const hello_o = m.fixtures.hello.objects[0];
writeFileSync(join(ar_dir, "tr.o"), new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 1, 5, 0]));
writeFileSync(join(ar_dir, "notes.txt"), "not an object\n");
copyFileSync(hello_o, join(ar_dir, "hello.o"));
execFileSync(m.ar, ["rcS", "--format=gnu", "bad.a", "tr.o", "notes.txt", "hello.o"], { cwd: ar_dir });
add("bad.a", readFileSync(join(ar_dir, "bad.a")));
add("notwasm", "hello world\n");
add("trunc", new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 1, 5, 0]));
add("v2", new Uint8Array([0, 0x61, 0x73, 0x6d, 2, 0, 0, 0]));
add("bc", new Uint8Array([0x42, 0x43, 0xc0, 0xde, 1, 2, 3, 4]));
add("h.wasm", readFileSync(m.fixtures.hello.reference));

// [command, llvm-size's arguments or null, the messages]
const ERRORS = [
    ["size bad.a", ["bad.a"],
     ["bad.a(tr.o): section at file offset 0x8: runs past the end of the file"]],
    ["size -A bad.a", ["-A", "bad.a"],
     ["bad.a(tr.o): section at file offset 0x8: runs past the end of the file"]],
    ["size h.wasm nosuch h.wasm", ["h.wasm", "nosuch", "h.wasm"],
     ["cannot open nosuch: not found"]],
    ["size notwasm", ["notwasm"], ["notwasm: not a wasm module"]],
    ["size trunc", ["trunc"],
     ["trunc: section at file offset 0x8: runs past the end of the file"]],
    ["size v2", ["v2"], ["v2: wasm version other than 1"]],
    ["size bc", ["bc"], ["bc: LLVM bitcode (from -flto), not a wasm module"]],
    ["size --radix=7 h.wasm", ["--radix=7", "h.wasm"],
     ["--radix value should be one of: 8, 10, 16"]],
    ["size --format=SysV h.wasm", ["--format=SysV", "h.wasm"],
     ["--format value should be one of: 'berkeley', 'darwin', 'sysv'"]],
    ["size --foo h.wasm", ["--foo", "h.wasm"], ["unknown argument '--foo'"]],
    ["size -Axq h.wasm", ["-Axq", "h.wasm"], ["unknown argument '-q'"]],
    ["size -format=sysv h.wasm", ["-format=sysv", "h.wasm"], ["unknown argument '-f'"]],
    ["size h.wasm --format", ["h.wasm", "--format"], ["--format: missing argument"]],
];
for (const [cmd, args, msgs] of ERRORS) {
    const want = llvm(args);
    const got = sh(cmd);
    const err = msgs.map((s) => `size: error: ${s}\n`).join("");
    if (got.status !== 1 || got.err !== err)
        bad.push(`${cmd}: status ${got.status}: ${JSON.stringify(got.err)}, expected ${JSON.stringify(err)}`);
    if (want.status !== 1)
        bad.push(`${cmd}: llvm-size's status is ${want.status}`);
    if (got.out !== want.out)
        bad.push(`${cmd}: ${differ(got.out, want.out)}`);
}

// No file: the usage, bare or asked for; with an option, an error.
for (const cmd of ["size", "size -h", "size --help"]) {
    const r = sh(cmd);
    if (r.status !== 0 || r.err || !r.out.startsWith("Usage:\n    size "))
        bad.push(`${cmd}: status ${r.status}: ${r.out}${r.err}`);
}
{
    const r = sh("size -A");
    if (r.status !== 1 || r.err !== "size: error: no input file specified\n")
        bad.push(`size -A: status ${r.status}: ${JSON.stringify(r.err)}`);
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`size ok: ${inputs.length} files in ${MODES.length} modes as llvm-size prints them, ` +
            `and ${ERRORS.length} errors`);
