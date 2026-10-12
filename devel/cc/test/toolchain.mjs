// The compiler on Braam, installed by pkg (ccfix.mjs): cc builds hello.c and
// runs it, compiles a few programs to the same assembly the host compiler
// makes of them, and reports a compile error with its status.

import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { HOST_VCC, TREE, fixture } from "./ccfix.mjs";

const { die, boot } = fixture("toolchain");
const { H, put, run, submit, store } = await boot();
const file = (path) => new Uint8Array(readFileSync(path));
let r;

// hello.c, built and run.
put("/tmp/hello.c", file(join(TREE, "test/hello.c")));
r = run("cc hello.c -o hello");
if (r.status !== 0)
    die(`cc hello.c: status ${r.status}\n${r.err}`);
r = run("./hello one two");
if (r.out !== "hello from ./hello: [one] [two]\n" || r.status !== 3)
    die(`hello: ${JSON.stringify(r)}`);

// The package's own example, as Manual.md has it.
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
