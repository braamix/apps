// The -M options on Braam, installed by pkg (ccfix.mjs): -MMD -MP beside a
// compile writes the rule the host compiler writes, -M lists the system
// headers -MM leaves out, a missing header fails and writes no rule, and -MG
// lists it instead.

import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { HOST_VCC, fixture } from "./ccfix.mjs";

const { die, boot } = fixture("deps");
const { put, run, text, H } = await boot();
let r;

const files = {
    "m.c": '#include "inc/a.h"\n#include <stdio.h>\nint x = A;\n',
    "inc/a.h": '#include "b.h"\n#define A B\n',
    "inc/b.h": "#define B 0\n",
};
const host = mkdtempSync(join(tmpdir(), "vcc-"));
mkdirSync(join(host, "inc"));
for (const [name, body] of Object.entries(files)) {
    writeFileSync(join(host, name), body);
    put(`/tmp/${name}`, body);
}

// Beside a compile, into the object's directory, as the host writes it.
const want = "obj/m.o: m.c inc/a.h inc/b.h\n\ninc/a.h:\n\ninc/b.h:\n";
mkdirSync(join(host, "obj"));
execFileSync(HOST_VCC, ["-MMD", "-MP", "-c", "m.c", "-o", "obj/m.o"], { cwd: host });
const got = readFileSync(join(host, "obj/m.d"), "utf8");
rmSync(host, { recursive: true });
if (got !== want)
    die(`host: ${JSON.stringify(got)}, expected ${JSON.stringify(want)}`);
r = run("mkdir obj; cc -MMD -MP -c m.c -o obj/m.o");
if (r.status !== 0)
    die(`cc -MMD: status ${r.status}\n${r.err}`);
if (text("/tmp/obj/m.d") !== want)
    die(`obj/m.d: ${JSON.stringify(text("/tmp/obj/m.d"))}, expected ${JSON.stringify(want)}`);
if (!H.store.files.has("/tmp/obj/m.o"))
    die("cc -MMD made no object");

// Instead of the compile: -MM as above, -M with the libc headers after.
r = run("cc -MM m.c");
if (r.status !== 0 || r.out !== "m.o: m.c inc/a.h inc/b.h\n")
    die(`cc -MM: ${JSON.stringify(r)}`);
r = run("cc -M m.c");
if (r.status !== 0 || !r.out.startsWith("m.o: m.c inc/a.h inc/b.h \\\n") ||
    !/\/include\/stdio\.h/.test(r.out))
    die(`cc -M: ${JSON.stringify(r)}`);

// A header that is not there: an error and no rule, or with -MG a rule.
put("/tmp/g.c", '#include "gen.h"\n');
r = run("cc -MD -c g.c");
if (r.status !== 1 || !r.err.includes("'gen.h' file not found") || H.store.files.has("/tmp/g.d"))
    die(`cc -MD g.c: ${JSON.stringify(r)}`);
r = run("cc -MM -MG g.c");
if (r.status !== 0 || r.out !== "g.o: g.c gen.h\n")
    die(`cc -MM -MG: ${JSON.stringify(r)}`);

console.log("deps ok: -MMD -MP as the host writes it, -M, -MM, and -MG");
