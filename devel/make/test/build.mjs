// make driving cc on Braam: a program of two sources and a header, built
// through the built-in .c.o rule, rebuilt after the header changes, and then
// up to date.  The cc package is installed as devel/cc's tests install it; the
// store's clock moves on at every write, so the times are real ones here.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { APPS, fixture } from "../../cc/test/ccfix.mjs";

const { die, boot } = fixture("make build");
const { H, put, run, submit } = await boot();
put("/bin/make", new Uint8Array(readFileSync(join(APPS, "build/devel/make/make.wasm"))));

// A file and a time for it: a store write stamps one, a planted file has none.
const file = (path, text) => {
    put(path, text);
    H.store.stamp(path);
};

file("/tmp/greet.h", "#include <braam.h>\n\ncoro(braam_call *) void greet(const char *who);\n");
file("/tmp/main.c", `#include "greet.h"

coro(braam_call *) int main(int argc, char **argv)
{
    await greet("make");
    return 0;
}
`);
const greet = (what) => `#include <stdio.h>
#include "greet.h"

coro(braam_call *) void greet(const char *who)
{
    await printf("${what} %s\\n", who);
}
`;
file("/tmp/greet.c", greet("hello from"));
file("/tmp/makefile", `hello: main.o greet.o
\tcc main.o greet.o -o hello
main.o greet.o: greet.h
`);
submit("cd /tmp");

let r = run("make");
const both = "cc  -c main.c\ncc  -c greet.c\ncc main.o greet.o -o hello\n";
if (r.status !== 0 || r.out !== both)
    die(`the first make: ${JSON.stringify(r)}`);
r = run("./hello");
if (r.status !== 0 || r.out !== "hello from make\n")
    die(`hello: ${JSON.stringify(r)}`);

r = run("make");
if (r.status !== 0 || r.out !== "`hello' is up to date.\n")
    die(`the second make: ${JSON.stringify(r)}`);

// A changed header remakes both objects and the program.
run("touch greet.h");
r = run("make");
if (r.status !== 0 || r.out !== both)
    die(`make after touch greet.h: ${JSON.stringify(r)}`);

// A changed source remakes its own object alone.
file("/tmp/greet.c", greet("goodbye from"));
r = run("make");
if (r.status !== 0 || r.out !== "cc  -c greet.c\ncc main.o greet.o -o hello\n")
    die(`make after greet.c changed: ${JSON.stringify(r)}`);
r = run("./hello");
if (r.out !== "goodbye from make\n")
    die(`hello after the remake: ${JSON.stringify(r)}`);

console.log("make build ok");
