// nm, held to llvm-nm byte for byte: every fixture's objects, archives and
// reference program, each program stripped, the SDK's libraries, ld.wasm,
// and two modules made here, in each format, order, radix and filter, one
// command line per mode. Then the errors: what llvm-nm prints beside them
// must still match, and the message is nm's own.

import { execFileSync, spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync }
    from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { boot, get, manifest, plant, run } from "../../ld/test/wasmlib.mjs";
import { foreign } from "./foreign.mjs";

function die(msg) {
    console.error("nm: " + msg);
    process.exit(1);
}

const m = manifest();
const NM = join(dirname(m.ld), "../nm/nm.wasm");
const LLVM_NM = join(dirname(m.objdump), "llvm-nm");
const LLVM_STRIP = join(dirname(m.objdump), "llvm-strip");
const tmp = mkdtempSync(join(tmpdir(), "nm-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

const H = await boot();
plant(H, "/bin/nm", new Uint8Array(readFileSync(NM)));
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
function sh(cmd) {
    for (const k of ["/tmp/o", "/tmp/e", "/tmp/s"])
        H.store.files.delete(k);
    plant(H, "/tmp/c", `cd /tmp/w; ${cmd} >/tmp/o 2>/tmp/e; echo $? >/tmp/s\n`);
    run(H, "sh /tmp/c");
    return { out: get(H, "/tmp/o") ?? "", err: get(H, "/tmp/e") ?? "", status: Number(get(H, "/tmp/s")) };
}

function llvm(args, input) {
    const r = spawnSync(LLVM_NM, args, { cwd: dir, encoding: "utf8", input, maxBuffer: 1 << 28 });
    if (r.error)
        die(`llvm-nm ${args.join(" ")}: ${r.error.message}`);
    return { out: r.stdout, err: r.stderr, status: r.status };
}

// The first line where two outputs part.
function differ(a, b) {
    const x = a.split("\n"), y = b.split("\n");
    for (let i = 0; i < Math.max(x.length, y.length); i++)
        if (x[i] !== y[i])
            return `line ${i + 1}: ${JSON.stringify(x[i])}, llvm-nm ${JSON.stringify(y[i])}`;
    return "";
}

// ------------------------------------------------------------ a module by hand

// A program with what the fixtures lack: an imported global, a global,
// table and memory exported, a passive segment, and, in the second copy,
// a name section naming functions, globals and segments.
const leb = (n) => {
    const b = [];
    do {
        let x = n & 0x7f;
        n >>>= 7;
        b.push(n ? x | 0x80 : x);
    } while (n);
    return b;
};
const str = (s) => [...leb(s.length), ...new TextEncoder().encode(s)];
const vec = (items) => [...leb(items.length), ...items.flat()];
const section = (id, body) => [id, ...leb(body.length), ...body];
const custom = (name, body) => section(0, [...str(name), ...body]);
const sub = (id, body) => [id, ...leb(body.length), ...body];
const names = (pairs) => vec(pairs.map(([i, n]) => [...leb(i), ...str(n)]));
const program = (named) => new Uint8Array([
    0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
    ...section(1, vec([[0x60, 0, 0]])),
    ...section(2, vec([[...str("env"), ...str("f"), 0, 0],
                       [...str("env"), ...str("g"), 3, 0x7f, 0]])),
    ...section(3, vec([[0], [0], [0]])),
    ...section(4, vec([[0x70, 0, 1]])),
    ...section(5, vec([[0, 1]])),
    ...section(6, vec([[0x7f, 1, 0x41, ...leb(1234), 0x0b], [0x7e, 0, 0x42, 5, 0x0b]])),
    ...section(7, vec([[...str("run"), 0, 1], [...str("gv"), 3, 1], [...str("t"), 1, 0],
                       [...str("mem"), 2, 0], [...str("gimp"), 3, 0]])),
    ...section(10, vec([[2, 0, 0x0b], [4, 0, 0x10, 0, 0x0b], [2, 0, 0x0b]])),
    ...section(11, vec([[0, 0x41, ...leb(1024), 0x0b, ...str("hello")], [1, ...str("xy")]])),
    ...(named ? custom("name", [
        ...sub(1, names([[0, "imported_f"], [1, "run"], [2, "helper"], [3, "zlast"]])),
        ...sub(7, names([[0, "g_imp"], [1, "g_one"], [2, "g_two"]])),
        ...sub(9, names([[0, ".rodata"], [1, ".passive"]])),
    ]) : []),
]);

// ------------------------------------------------------------ llvm-nm's output

const inputs = [add("ld.wasm", readFileSync(m.ld))];
inputs.push(add("hand.wasm", program(false)), add("named.wasm", program(true)));
let n = 0;
const seen = new Set();
const strip = (from, to) => execFileSync(LLVM_STRIP, [from, "-o", to]);
for (const [name, fx] of Object.entries(m.fixtures)) {
    inputs.push(add(`fx_${name}.wasm`, readFileSync(fx.reference)));
    strip(fx.reference, join(tmp, "s.wasm"));
    inputs.push(add(`fx_${name}.stripped`, readFileSync(join(tmp, "s.wasm"))));
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
strip(m.ld, join(tmp, "s.wasm"));
inputs.push(add("ld.stripped", readFileSync(join(tmp, "s.wasm"))));
for (const f of readdirSync(m.sdk_libs).filter((n) => n.endsWith(".a")).sort())
    inputs.push(add(f, readFileSync(join(m.sdk_libs, f))));
{
    const fdir = join(tmp, "foreign");
    mkdirSync(fdir);
    for (const f of foreign(m, fdir))
        inputs.push(add(f, readFileSync(join(fdir, f))));
}

let lines = 0;
const MODES = [[], ["-A"], ["-P"], ["-P", "-A"], ["-P", "-td"], ["-f", "sysv"], ["-fsysv", "-A"],
               ["-j"], ["-m"], ["-n"], ["-S"], ["-S", "-n"], ["-r"], ["-p"], ["-r", "-n"],
               ["--size-sort"], ["-S", "--size-sort"], ["-r", "--size-sort"], ["-g"], ["-u"],
               ["-U"], ["-W"], ["-C"], ["-C", "--no-demangle"], ["-to"], ["-t", "d", "-S"],
               ["-M"], ["-a", "--special-syms"], ["-gUS"], ["--format=just-symbols", "-A"],
               ["-f", "posix", "-B"], ["--export-symbols"], ["--export-symbols", "-n"]];
for (const mode of MODES) {
    const want = llvm([...mode, ...inputs]);
    if (want.status !== 0 || want.err)
        die(`llvm-nm ${mode.join(" ")}: status ${want.status}: ${want.err}`);
    const got = sh(`nm ${mode.join(" ")} ${inputs.join(" ")}`);
    if (got.status !== 0 || got.err)
        bad.push(`[${mode.join(" ")}]: status ${got.status}: ${got.err.slice(0, 500)}`);
    else if (got.out !== want.out)
        bad.push(`[${mode.join(" ")}]: ${differ(got.out, want.out)}`);
    lines += want.out.split("\n").length;
}

// One file on its own, so no label; standard input; a.out when no file is
// named.
{
    const prog = readFileSync(m.fixtures.hello.reference);
    for (const mode of [[], ["-A"], ["-f", "sysv"]]) {
        const want = llvm([...mode, "named.wasm"]);
        const got = sh(`nm ${mode.join(" ")} named.wasm`);
        if (got.status !== 0 || got.out !== want.out)
            bad.push(`one file [${mode.join(" ")}]: ${got.status} ${got.err} ${differ(got.out, want.out)}`);
    }
    plant(H, "/tmp/in", new Uint8Array(prog));
    for (const mode of [[], ["-A"]]) {
        const want = llvm([...mode, "-"], prog);
        const got = sh(`nm ${mode.join(" ")} - </tmp/in`);
        if (got.status !== 0 || got.out !== want.out)
            bad.push(`stdin [${mode.join(" ")}]: ${got.status} ${got.err} ${differ(got.out, want.out)}`);
    }
    add("a.out", prog);
    const want = llvm([]);
    const got = sh("nm");
    if (got.status !== 0 || got.out !== want.out)
        bad.push(`a.out: ${got.status} ${got.err} ${differ(got.out, want.out)}`);
    rmSync(join(dir, "a.out"));
    H.store.files.delete("/tmp/w/a.out");
}

// ------------------------------------------------------------ errors

// An archive with a malformed member, a text member and an object: the
// object is listed, the text skipped, the malformed one an error.
const ar_dir = join(tmp, "ar");
mkdirSync(ar_dir);
writeFileSync(join(ar_dir, "tr.o"), new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 1, 5, 0]));
writeFileSync(join(ar_dir, "notes.txt"), "not an object\n");
copyFileSync(m.fixtures.hello.objects[0], join(ar_dir, "hello.o"));
execFileSync(m.ar, ["rcS", "--format=gnu", "bad.a", "tr.o", "notes.txt", "hello.o"], { cwd: ar_dir });
add("bad.a", readFileSync(join(ar_dir, "bad.a")));
add("notwasm", "hello world\n");
add("trunc", new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 1, 5, 0]));
add("bc", new Uint8Array([0x42, 0x43, 0xc0, 0xde, 1, 2, 3, 4]));
add("empty", new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));
add("h.o", readFileSync(m.fixtures.hello.objects[0]));

// [command, llvm-nm's arguments, nm's stderr]
const e = (s) => `nm: error: ${s}\n`;
const ERRORS = [
    ["nm bad.a", ["bad.a"],
     e("bad.a(tr.o): section at file offset 0x8: runs past the end of the file")],
    ["nm -A bad.a", ["-A", "bad.a"],
     e("bad.a(tr.o): section at file offset 0x8: runs past the end of the file")],
    ["nm h.o nosuch h.o", ["h.o", "nosuch", "h.o"], e("cannot open nosuch: not found")],
    ["nm notwasm", ["notwasm"], e("notwasm: not a wasm module")],
    ["nm trunc", ["trunc"], e("trunc: section at file offset 0x8: runs past the end of the file")],
    ["nm --radix=q named.wasm", ["--radix=q", "named.wasm"],
     e("--radix value should be one of: 'o' (octal), 'd' (decimal), 'x' (hexadecimal)")],
    ["nm -f foo h.o", ["-f", "foo", "h.o"],
     e("--format value should be one of: bsd, posix, sysv, darwin, just-symbols")],
    ["nm -X foo h.o", ["-X", "foo", "h.o"],
     e("-X value should be one of: 32, 64, 32_64, (default) any")],
    ["nm --foo h.o", ["--foo", "h.o"], e("unknown argument '--foo'")],
    ["nm -ge h.o", ["-ge", "h.o"], e("unknown argument '-e'")],
    ["nm h.o -t", ["h.o", "-t"], e("-t: missing argument")],
    ["nm -D h.o", ["-D", "h.o"], e("h.o: File format has no dynamic symbol table")],
];
for (const [cmd, args, err] of ERRORS) {
    const want = llvm(args);
    const got = sh(cmd);
    if (got.status !== 1 || got.err !== err)
        bad.push(`${cmd}: status ${got.status}: ${JSON.stringify(got.err)}, expected ${JSON.stringify(err)}`);
    if (want.status !== 1)
        bad.push(`${cmd}: llvm-nm's status is ${want.status}`);
    if (got.out !== want.out)
        bad.push(`${cmd}: ${differ(got.out, want.out)}`);
}

// A module with no symbols is noted, and is no error; --quiet drops the note.
for (const [cmd, args] of [["nm empty", ["empty"]], ["nm -A empty h.o", ["-A", "empty", "h.o"]],
                           ["nm --quiet empty", ["--quiet", "empty"]]]) {
    const want = llvm(args);
    const got = sh(cmd);
    if (got.status !== 0 || got.err !== want.err || got.out !== want.out)
        bad.push(`${cmd}: status ${got.status}: ${JSON.stringify(got.err)}, llvm-nm ` +
                 `${JSON.stringify(want.err)} ${differ(got.out, want.out)}`);
}

{
    const r = sh("nm --help");
    if (r.status !== 0 || !r.out.startsWith("usage: nm"))
        bad.push(`--help: status ${r.status}: ${r.out}${r.err}`);
}

// llvm-nm reads bitcode; nm refuses it.
{
    const r = sh("nm bc");
    if (r.status !== 1 || r.err !== e("bc: LLVM bitcode (from -flto), not a wasm module"))
        bad.push(`nm bc: status ${r.status}: ${r.err}`);
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`nm ok: ${inputs.length} files in ${MODES.length} modes as llvm-nm prints them ` +
            `(${lines} lines), and ${ERRORS.length + 1} errors`);
