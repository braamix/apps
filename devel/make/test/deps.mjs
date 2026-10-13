// Automatic dependencies, both ways GNU make has them, with cc on Braam.
//
// The new way: cc -MMD -MP writes a .d beside each object, and the makefile
// -includes them.  The first build has none and builds everything; then a
// header moved remakes exactly the objects that include it; then a header
// deleted, and dropped from the source, does not stop make, -MP's empty rule
// standing in for it.
//
// The old way: a %.d: %.c rule writes them, and make remakes its own
// makefiles -- the included .d files -- and starts again to read them.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { APPS, fixture } from "../../cc/test/ccfix.mjs";

const { die, boot } = fixture("make deps");
const { H, put, run } = await boot();
put("/bin/make", new Uint8Array(readFileSync(join(APPS, "build/devel/make/make.wasm"))));

// A file and a time for it: a store write stamps one, a planted file has none.
const file = (path, text) => {
    put(path, text);
    H.store.stamp(path);
};
const lines = (out) => out.split("\n").filter((l) => l !== "");
const same = (a, b) => [...a].sort().join("\n") === [...b].sort().join("\n");

// Three objects: main.c includes both headers, one.c one.h, two.c two.h.
const sources = () => {
    file("/tmp/one.h", "#include <braam.h>\ncoro(braam_call *) void one(void);\n");
    file("/tmp/two.h", "#include <braam.h>\ncoro(braam_call *) void two(void);\n");
    for (const n of ["one", "two"])
        file(`/tmp/${n}.c`, `#include <stdio.h>
#include "${n}.h"

coro(braam_call *) void ${n}(void)
{
    await printf("${n}\\n");
}
`);
    file("/tmp/main.c", `#include "one.h"
#include "two.h"

coro(braam_call *) int main(int argc, char **argv)
{
    await one();
    await two();
    return 0;
}
`);
};

const OBJS = ["main.o", "one.o", "two.o"];
const compile = (o, flags) => `cc ${flags} -c ${o.replace(/\.o$/, ".c")}`;
const link = `cc -o prog ${OBJS.join(" ")}`;

// --- The new way: -MMD -MP.
sources();
file("/tmp/makefile", `CFLAGS += -MMD -MP
OBJS = ${OBJS.join(" ")}

prog: $(OBJS)
\t$(CC) -o $@ $(OBJS)

-include $(OBJS:.o=.d)
`);
const md = (o) => compile(o, " -MMD -MP");

let r = run("make"), said;
if (r.status !== 0 || lines(r.out).at(-1) !== link ||
    !same(lines(r.out).slice(0, -1), OBJS.map(md)))
    die(`the first make: ${JSON.stringify(r)}`);
for (const o of OBJS)
    if (!H.store.files.has(`/tmp/${o.replace(/\.o$/, ".d")}`))
        die(`cc -MMD wrote no ${o.replace(/\.o$/, ".d")}`);
r = run("make");
if (r.status !== 0 || r.out !== "`prog' is up to date.\n")
    die(`the second make: ${JSON.stringify(r)}`);

// one.h moves: main.o and one.o, not two.o.
file("/tmp/one.h", "#include <braam.h>\ncoro(braam_call *) void one(void);\n\n");
r = run("make");
if (r.status !== 0 || lines(r.out).at(-1) !== link ||
    !same(lines(r.out).slice(0, -1), ["main.o", "one.o"].map(md)))
    die(`make after one.h moved: ${JSON.stringify(r)}`);

// two.h goes, and main.c and two.c no longer include it.
H.store.files.delete("/tmp/two.h");
const text = (path) => new TextDecoder().decode(H.store.files.get(path));
file("/tmp/main.c", text("/tmp/main.c")
         .replace('#include "two.h"\n', "coro(braam_call *) void two(void);\n"));
file("/tmp/two.c", text("/tmp/two.c").replace('#include "two.h"', "#include <braam.h>"));
r = run("make");
if (r.status !== 0 || lines(r.out).at(-1) !== link ||
    !same(lines(r.out).slice(0, -1), ["main.o", "two.o"].map(md)))
    die(`make after two.h went: ${JSON.stringify(r)}`);
r = run("./prog");
if (r.status !== 0 || r.out !== "one\ntwo\n")
    die(`prog: ${JSON.stringify(r)}`);

// --- The old way: the .d files are a rule's, and make remakes them as its
// own makefiles, then starts again to read them.
for (const f of [...H.store.files.keys()])
    if (/^\/tmp\/[^/]*\.[odh]$/.test(f) || f === "/tmp/prog")
        H.store.files.delete(f);
sources();
file("/tmp/makefile", `OBJS = ${OBJS.join(" ")}

prog: $(OBJS)
\t$(CC) -o $@ $(OBJS)

%.d: %.c
\t$(CC) -MM -MF $@ $<

-include $(OBJS:.o=.d)
`);
const dep = (o) => `cc -MM -MF ${o.replace(/\.o$/, ".d")} ${o.replace(/\.o$/, ".c")}`;
const plain = (o) => compile(o, "");

// The .d files are made, make starts again, and then the objects.
r = run("make");
said = lines(r.out);
if (r.status !== 0 || !same(said.slice(0, 3), OBJS.map(dep)) || said.at(-1) !== link ||
    !same(said.slice(3, -1), OBJS.map(plain)))
    die(`the first make, with %.d: ${JSON.stringify(r)}`);
r = run("make");
if (r.status !== 0 || r.out !== "`prog' is up to date.\n")
    die(`the second make, with %.d: ${JSON.stringify(r)}`);

// two.h moves: no .d is remade, and main.o and two.o are.
file("/tmp/two.h", "#include <braam.h>\ncoro(braam_call *) void two(void);\n\n");
r = run("make");
said = lines(r.out);
if (r.status !== 0 || said.at(-1) !== link ||
    !same(said.slice(0, -1), ["main.o", "two.o"].map(plain)))
    die(`make after two.h moved, with %.d: ${JSON.stringify(r)}`);

console.log("make deps ok");
