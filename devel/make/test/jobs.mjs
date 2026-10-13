// A program of five sources built with cc on Braam under -j 4: every object
// compiled before the link, the program the same bytes as under -j 1, nothing
// made on the run after, and a failing compile under -j waiting for the ones
// running before it stops.
// ^C under -j is cases.mjs's jobsint.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { APPS, fixture } from "../../cc/test/ccfix.mjs";

const { die, boot } = fixture("make jobs");
const { H, put, run } = await boot();
put("/bin/make", new Uint8Array(readFileSync(join(APPS, "build/devel/make/make.wasm"))));

// A file and a time for it: a store write stamps one, a planted file has none.
const file = (path, text) => {
    put(path, text);
    H.store.stamp(path);
};

const parts = ["a", "b", "c", "d"];
file("/tmp/makefile", `OBJS = main.o ${parts.map((p) => p + ".o").join(" ")}

prog: $(OBJS)
\t$(CC) -o $@ $(OBJS)

%.o: %.c parts.h
\t$(CC) -c $<

broken: a.o bad.o b.o
`);
file("/tmp/parts.h", `#include <braam.h>

${parts.map((p) => `coro(braam_call *) void part_${p}(void);`).join("\n")}
`);
for (const p of parts)
    file(`/tmp/${p}.c`, `#include <stdio.h>
#include "parts.h"

coro(braam_call *) void part_${p}(void)
{
    await printf("part ${p}\\n");
}
`);
file("/tmp/main.c", `#include "parts.h"

coro(braam_call *) int main(int argc, char **argv)
{
${parts.map((p) => `    await part_${p}();`).join("\n")}
    return 0;
}
`);
file("/tmp/bad.c", "this is not C\n");

const lines = (out) => out.split("\n").filter((l) => l !== "");
const compiles = ["main", ...parts].map((p) => `cc -c ${p}.c`);
const link = `cc -o prog main.o ${parts.map((p) => p + ".o").join(" ")}`;
const want = parts.map((p) => `part ${p}`).join("\n") + "\n";

let r = run("make -j 4");
let said = lines(r.out);
if (r.status !== 0 || said.length !== compiles.length + 1 || said.at(-1) !== link ||
    [...said.slice(0, -1)].sort().join("\n") !== [...compiles].sort().join("\n"))
    die(`make -j 4: ${JSON.stringify(r)}`);
r = run("./prog");
if (r.status !== 0 || r.out !== want)
    die(`prog under -j 4: ${JSON.stringify(r)}`);
const parallel = H.store.files.get("/tmp/prog");

r = run("make -j 4");
if (r.status !== 0 || r.out !== "`prog' is up to date.\n")
    die(`make -j 4 again: ${JSON.stringify(r)}`);

// One at a time, in the order written, and the same program.
run(`rm prog ${["main", ...parts].map((p) => p + ".o").join(" ")}`);
r = run("make -j 1");
if (r.status !== 0 || lines(r.out).join("\n") !== [...compiles, link].join("\n"))
    die(`make -j 1: ${JSON.stringify(r)}`);
if (Buffer.compare(Buffer.from(parallel), Buffer.from(H.store.files.get("/tmp/prog"))) !== 0)
    die("prog under -j 1 is not the program -j 4 made");

// A compile fails: the others running finish, nothing more is begun, and
// make stops.
run("rm a.o b.o");
r = run("make -j 3 broken");
said = lines(r.out);
if (r.status !== 2 || !said.includes("cc -c bad.c") || !said.includes("*** Error code 1") ||
    said.at(-1) !== "Stop." && !/Stop\./.test(r.err + r.out))
    die(`make -j 3 broken: ${JSON.stringify(r)}`);
if (!H.store.files.has("/tmp/a.o"))
    die("a.o, begun before bad.c failed, was not finished");

console.log("make jobs ok");
