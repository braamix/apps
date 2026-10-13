// A library made through POSIX's .c.a rule with cc and ar on Braam, a program
// linked against it, nothing made on the run after, and one member remade when
// its source moves. Phase B's real build, as portable.mjs is phase A's.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { APPS, fixture } from "../../cc/test/ccfix.mjs";

const { die, boot } = fixture("make library");
const { H, put, run } = await boot();
put("/bin/make", new Uint8Array(readFileSync(join(APPS, "build/devel/make/make.wasm"))));

// A file and a time for it: a store write stamps one, a planted file has none.
const file = (path, text) => {
    put(path, text);
    H.store.stamp(path);
};

// ccfix's run() keeps its result files in /tmp, so the project is there.
file("/tmp/makefile", `.POSIX:
LIB = libgreet.a

hello: main.o $(LIB)
\t$(CC) $(LDFLAGS) -o $@ main.o $(LIB)
$(LIB): $(LIB)(greet.o) $(LIB)(shout.o)
main.o $(LIB)(greet.o) $(LIB)(shout.o): greet.h
`);
file("/tmp/greet.h", `#include <braam.h>

coro(braam_call *) void greet(const char *who);
const char *shout(void);
`);
file("/tmp/greet.c", `#include <stdio.h>
#include "greet.h"

coro(braam_call *) void greet(const char *who)
{
    await printf("hello from %s%s\\n", who, shout());
}
`);
const shout = (mark) => `#include "greet.h"

const char *shout(void)
{
    return "${mark}";
}
`;
file("/tmp/shout.c", shout("!"));
file("/tmp/main.c", `#include "greet.h"

coro(braam_call *) int main(int argc, char **argv)
{
    await greet("a library");
    return 0;
}
`);

const member = (m) => [`cc -c  ${m}.c`, `ar -rvU libgreet.a ${m}.o`, `rm ${m}.o`];
const link = "cc  -o hello main.o libgreet.a";

let r = run("make");
const want = ["cc  -c main.c", ...member("greet"), ...member("shout"), link];
const said = r.out.split("\n").filter((l) => !/^[ar] - /.test(l) && l !== "");
if (r.status !== 0 || said.join("\n") !== want.join("\n"))
    die(`the first make: ${JSON.stringify(r)}`);
r = run("./hello");
if (r.status !== 0 || r.out !== "hello from a library!\n")
    die(`hello: ${JSON.stringify(r)}`);
if (H.store.files.has("/tmp/greet.o") || H.store.files.has("/tmp/shout.o"))
    die("the .c.a rule left its objects behind");

r = run("make");
if (r.status !== 0 || r.out !== "`hello' is up to date.\n")
    die(`the second make: ${JSON.stringify(r)}`);

// One member's source moves: that member alone, and the link.
file("/tmp/shout.c", shout("."));
r = run("make");
const again = r.out.split("\n").filter((l) => !/^[ar] - /.test(l) && l !== "");
if (r.status !== 0 || again.join("\n") !== [...member("shout"), link].join("\n"))
    die(`make after shout.c moved: ${JSON.stringify(r)}`);
r = run("./hello");
if (r.out !== "hello from a library.\n")
    die(`hello after the remake: ${JSON.stringify(r)}`);

console.log("make library ok");
