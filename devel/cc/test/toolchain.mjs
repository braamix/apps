// The compiler on Braam: vcc and its passes installed in /opt/vcc, as and ld
// from devel/wasm in /bin. It builds hello.c and runs it, compiles a few
// programs to the same assembly the host compiler makes of them, and reports a
// compile error with its status.
//
// A pass that ends through exit() needs a kernel that delivers what such a
// process wrote in its last step (braam-core's System_Calls.md §5); on one that
// does not, this says so and checks nothing more.

import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS } from "../../../test/sdk.mjs";

const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const TREE = join(APPS, "devel/cc");
const BUILD = join(APPS, "build/devel/cc");
const HOST_VCC = join(APPS, "build/host-cc/stage/bin/vcc");

function die(msg) {
    console.error("toolchain: " + msg);
    process.exit(1);
}

const H = await import(HARNESS);
await H.init(KERNEL, ROOTFS);
H.kernel().init(0);
if (H.run(0) !== -1)
    die("the shell did not park on the keyboard");
H.regrid(120, 40, "resize returned no screen descriptor");

function put(path, bytes) {
    H.store.files.set(path, typeof bytes === "string" ? new TextEncoder().encode(bytes) : bytes);
    for (let d = dirname(path); d !== "/"; d = dirname(d))
        H.store.dirs.add(d);
}
const file = (path) => new Uint8Array(readFileSync(path));
const text = (path) => new TextDecoder().decode(H.store.files.get(path) ?? new Uint8Array());

let now = 1;
function submit(line) {
    if (line.length > 60)
        die(`the command line is ${line.length} keys, and the ring holds 64`);
    H.submit(line, now++);
    if (H.run(now++) !== -1)
        die("the kernel did not settle after: " + line);
}

// A command's stdout, stderr and status, by way of three files in the working
// directory: short names, as a command line is at most sixty keys.
let cwd = "/home";
function run(cmd) {
    submit(`${cmd} >o 2>e; echo $? >s`);
    return { out: text(cwd + "/o"), err: text(cwd + "/e"), status: Number(text(cwd + "/s").trim()) };
}

// Does exit() deliver?
put("/bin/sys", file(join(BUILD, "systest.wasm")));
const probe = run("sys exit");
if (probe.status !== 3) {
    if (probe.status === 132) {
        console.log("toolchain skipped: this kernel drops what exit() leaves");
        process.exit(0);
    }
    die(`sys exit: status ${probe.status}, ${JSON.stringify(probe)}`);
}
if (probe.out !== "before exit\n" || probe.err !== "on stderr\n")
    die(`exit() lost its output: ${JSON.stringify(probe)}`);

// The installation, as upstream lays it out but for the passes, in libexec/.
put("/opt/vcc/bin/vcc", file(join(BUILD, "vcc.wasm")));
for (const [p, n] of [["cpp", "vcpp"], ["parse", "vparse"], ["lower", "vlower"], ["genwasm", "vgenwasm"]])
    put("/opt/vcc/libexec/" + n, file(join(BUILD, p + ".wasm")));
const share = "/opt/vcc/share/vcc/wasm32-braam/";
const include = join(APPS, "build/host-cc/stage/share/vcc/wasm32-braam/include");
(function walk(dir, to) {
    for (const n of readdirSync(dir)) {
        const s = join(dir, n);
        if (statSync(s).isDirectory())
            walk(s, to + n + "/");
        else
            put(to + n, file(s));
    }
})(include, share + "include/");
put(share + "lib/crt0.o", file(join(BUILD, "lib/crt0.o")));
put(share + "lib/libc.a", file(join(BUILD, "lib/libc.a")));
put("/bin/as", file(join(APPS, "build/devel/wasm/as/as.wasm")));
put("/bin/ld", file(join(APPS, "build/devel/wasm/ld/ld.wasm")));
submit("cd /tmp");
cwd = "/tmp";

// hello.c, built and run.
put("/tmp/hello.c", file(join(TREE, "test/hello.c")));
let r = run("/opt/vcc/bin/vcc hello.c -o hello");
if (r.status !== 0)
    die(`vcc hello.c: status ${r.status}\n${r.err}`);
r = run("./hello one two");
if (r.out !== "hello from ./hello: [one] [two]\n" || r.status !== 3)
    die(`hello: ${JSON.stringify(r)}`);

// The same assembly as the host's compiler.
const host = mkdtempSync(join(tmpdir(), "vcc-"));
const programs = ["test/hello.c", "docs/examples/notes.c", "test/programs/braam_tests/CopyFile.c",
                  "test/programs/braam_tests/Files.c", "test/programs/braam_tests/Tasks.c",
                  "test/programs/braam_tests/Poll.c"];
let compared = 0;
for (const [k, src] of programs.entries()) {
    const source = readFileSync(join(TREE, src));
    writeFileSync(join(host, `p${k}.c`), source);
    execFileSync(HOST_VCC, ["-S", `p${k}.c`, "-o", `p${k}.s`], { cwd: host });
    put(`/tmp/p${k}.c`, new Uint8Array(source));
    r = run(`/opt/vcc/bin/vcc -S p${k}.c -o p${k}.s`);
    if (r.status !== 0)
        die(`vcc -S ${src}: status ${r.status}\n${r.err}`);
    if (Buffer.compare(Buffer.from(H.store.files.get(`/tmp/p${k}.s`)),
                       readFileSync(join(host, `p${k}.s`))) !== 0)
        die(`${src}: the assembly differs from the host's`);
    compared++;
}

// A compile error: its first line as the host says it, and status 1.
const bad = "int main(void)\n{\n    return x;\n}\n";
writeFileSync(join(host, "bad.c"), bad);
let want = "";
try {
    execFileSync(HOST_VCC, ["-c", "bad.c"], { cwd: host, stdio: ["ignore", "ignore", "pipe"] });
} catch (e) {
    want = e.stderr.toString().split("\n")[0];
}
rmSync(host, { recursive: true });
put("/tmp/bad.c", bad);
r = run("/opt/vcc/bin/vcc -c bad.c");
if (r.status !== 1 || r.err.split("\n")[0] !== want)
    die(`bad.c: status ${r.status}, ${JSON.stringify(r.err)}, expected ${JSON.stringify(want)}`);

console.log(`toolchain ok: hello built and run, ${compared} programs as the host compiles them, ` +
            "and an error");
