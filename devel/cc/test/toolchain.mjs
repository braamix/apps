// The compiler on Braam, as pkg installs it: a signed index offering the cc
// and wasm packages, `pkg install cc`, which pulls in wasm, and then cc builds
// hello.c and runs it, compiles a few programs to the same assembly the host
// compiler makes of them, and reports a compile error with its status.
//
// The anchor is one of the test's own, over keys made for this run, so no
// publisher's key is needed. A pass that ends through exit() needs a kernel
// that delivers what such a process wrote in its last step (braam-core's
// System_Calls.md §5); on one that does not, this checks the install alone.

import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HARNESS, KERNEL, ROOTFS, SDK } from "../../../test/sdk.mjs";

const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const TREE = join(APPS, "devel/cc");
const BUILD = join(APPS, "build/devel/cc");
const HOST_VCC = join(APPS, "build/host-cc/stage/bin/vcc");
const TOOLS = join(SDK, "libexec/braam");
const URL = "https://braamix.github.io";

function die(msg) {
    console.error("toolchain: " + msg);
    process.exit(1);
}

// The two packages, and a repository vouching for them.
execFileSync("cmake", ["--build", join(APPS, "build"), "--target", "pkg_cc", "pkg_wasm"],
             { stdio: ["ignore", "ignore", "inherit"] });
const zipIn = (dir, name) => {
    const z = readdirSync(dir).filter((f) => f.startsWith(name + "-") && f.endsWith(".zip"));
    if (z.length !== 1)
        die(`${dir}: ${z.length} ${name} packages`);
    return join(dir, z[0]);
};
const zips = [zipIn(BUILD, "cc"), zipIn(join(APPS, "build/devel/wasm"), "wasm")];
const keys = mkdtempSync(join(tmpdir(), "vcc-repo-"));
const py = (script, args) => execFileSync("python3", [join(TOOLS, script), ...args], { cwd: keys });
execFileSync("python3", ["-c", "import sys, ed25519; ed25519.generate('root.key'); " +
                         "ed25519.generate('index.key')"],
             { cwd: keys, env: { ...process.env, PYTHONPATH: TOOLS } });
py("mkanchor.py", ["--out", "anchor", "--version", "3", "--expiry", "4102444800000",
                   "--threshold", "root=1", "--threshold", "index=1", "--key", "root=root.key",
                   "--key", "index=index.key", "--sign", "root.key"]);
py("mkindex.py", ["--out", "index", "--url", URL, "--version", "1", "--expiry", "4102444800000",
                  "--sign", "index.key", ...zips]);
const anchor = readFileSync(join(keys, "anchor"));
const index = readFileSync(join(keys, "index"));
rmSync(keys, { recursive: true });

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

put("/etc/anchor", new Uint8Array(anchor));
put("/etc/repositories", URL + "\n");
H.net.routes.set(URL + "/index",
                 { status: 200, headers: "content-type: text/plain\n", body: new Uint8Array(index) });
for (const z of zips)
    H.net.routes.set(`${URL}/${z.slice(z.lastIndexOf("/") + 1)}`,
                     { status: 200, headers: "content-type: application/zip\n",
                       body: new Uint8Array(readFileSync(z)) });

// pkg install cc, which needs wasm.
let r = run("pkg update");
if (r.status !== 0)
    die(`pkg update: status ${r.status}\n${r.out}${r.err}`);
r = run("pkg install cc");
if (r.status !== 0 || !/Installing wasm /.test(r.out) || !/Installing cc /.test(r.out))
    die(`pkg install cc: status ${r.status}\n${r.out}${r.err}`);
for (const c of ["cc", "as", "ld"])
    if (!H.store.files.has("/pkg/gen/1/bin/" + c))
        die(`no ${c} in /pkg/bin`);

// Does exit() deliver?
put("/bin/sys", file(join(BUILD, "systest.wasm")));
const probe = run("sys exit");
if (probe.status !== 3) {
    if (probe.status === 132) {
        console.log("toolchain: cc installed; nothing compiled, as this kernel drops what " +
                    "exit() leaves");
        process.exit(0);
    }
    die(`sys exit: status ${probe.status}, ${JSON.stringify(probe)}`);
}
if (probe.out !== "before exit\n" || probe.err !== "on stderr\n")
    die(`exit() lost its output: ${JSON.stringify(probe)}`);
submit("cd /tmp");
cwd = "/tmp";

// hello.c, built and run.
put("/tmp/hello.c", file(join(TREE, "test/hello.c")));
r = run("cc hello.c -o hello");
if (r.status !== 0)
    die(`cc hello.c: status ${r.status}\n${r.err}`);
r = run("./hello one two");
if (r.out !== "hello from ./hello: [one] [two]\n" || r.status !== 3)
    die(`hello: ${JSON.stringify(r)}`);

// The package's own example, as Manual.md has it.
const store = "/pkg/store/" + zips[0].slice(zips[0].lastIndexOf("/") + 1, -4);
submit(`cp ${store}/share/hello.c greet.c`);
r = run("cc greet.c && ./a.out");
if (r.out !== "Hello, world!\n" || r.status !== 0)
    die(`share/hello.c: ${JSON.stringify(r)}`);

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
    r = run(`cc -S p${k}.c -o p${k}.s`);
    if (r.status !== 0)
        die(`cc -S ${src}: status ${r.status}\n${r.err}`);
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
r = run("cc -c bad.c");
if (r.status !== 1 || r.err.split("\n")[0] !== want)
    die(`bad.c: status ${r.status}, ${JSON.stringify(r.err)}, expected ${JSON.stringify(want)}`);

console.log(`toolchain ok: cc installed, hello built and run, ${compared} programs as the ` +
            "host compiles them, and an error");
