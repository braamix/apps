// A C program built with cc on Braam from a makefile written for GNU make:
// $(wildcard) and $(patsubst) for the objects, a pattern rule, VPATH for the
// sources, an order-only object directory, and a conditional on DEBUG.  Then
// nothing made on the run after, and one object remade when its source moves.
// Phase C's real build, as library.mjs is phase B's.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { APPS, fixture } from "../../cc/test/ccfix.mjs";

const { die, boot } = fixture("make gnu");
const { H, put, run } = await boot();
put("/bin/make", new Uint8Array(readFileSync(join(APPS, "build/devel/make/make.wasm"))));

// A file and a time for it: a store write stamps one, a planted file has none.
const file = (path, text) => {
    put(path, text);
    H.store.stamp(path);
};

// ccfix's run() keeps its result files in /tmp, so the project is there.
file("/tmp/makefile", `VPATH = src
OBJDIR = obj
SRCS := $(notdir $(wildcard src/*.c))
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
ifdef DEBUG
CFLAGS += -DLOUD
endif

hello: $(OBJS)
\t$(CC) -o $@ $^

$(OBJDIR)/%.o: %.c greet.h | $(OBJDIR)
\t$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
\tmkdir $@
`);
file("/tmp/src/greet.h", `#include <braam.h>

coro(braam_call *) void greet(const char *who);
`);
const greet = (mark) => `#include <stdio.h>
#include "greet.h"

coro(braam_call *) void greet(const char *who)
{
#ifdef LOUD
    await printf("HELLO FROM %s${mark}\\n", who);
#else
    await printf("hello from %s${mark}\\n", who);
#endif
}
`;
file("/tmp/src/greet.c", greet("!"));
file("/tmp/src/main.c", `#include "greet.h"

coro(braam_call *) int main(int argc, char **argv)
{
    await greet("gnu make");
    return 0;
}
`);

const compile = (m, flags = "  ") => `cc${flags}-c -o obj/${m}.o src/${m}.c`;
const link = "cc -o hello obj/greet.o obj/main.o";
const lines = (out) => out.split("\n").filter((l) => l !== "");

let r = run("make");
let want = ["mkdir obj", compile("greet"), compile("main"), link];
if (r.status !== 0 || lines(r.out).join("\n") !== want.join("\n"))
    die(`the first make: ${JSON.stringify(r)}`);
r = run("./hello");
if (r.status !== 0 || r.out !== "hello from gnu make!\n")
    die(`hello: ${JSON.stringify(r)}`);

r = run("make");
if (r.status !== 0 || r.out !== "`hello' is up to date.\n")
    die(`the second make: ${JSON.stringify(r)}`);

// One source moves: its object alone, and the link.
file("/tmp/src/greet.c", greet("."));
r = run("make");
if (r.status !== 0 || lines(r.out).join("\n") !== [compile("greet"), link].join("\n"))
    die(`make after greet.c moved: ${JSON.stringify(r)}`);
r = run("./hello");
if (r.out !== "hello from gnu make.\n")
    die(`hello after the remake: ${JSON.stringify(r)}`);

// The conditional: DEBUG from the command line, and everything made again.
r = run("rm -r obj hello");
r = run("make DEBUG=1");
want = ["mkdir obj", compile("greet", "  -DLOUD "), compile("main", "  -DLOUD "), link];
if (r.status !== 0 || lines(r.out).join("\n") !== want.join("\n"))
    die(`make DEBUG=1: ${JSON.stringify(r)}`);
r = run("./hello");
if (r.out !== "HELLO FROM gnu make.\n")
    die(`hello under DEBUG: ${JSON.stringify(r)}`);

console.log("make gnu ok");
