// A program of three directories built with cc on Braam from makefiles in the
// portable style phase A of Plan.md is for: `cd dir && $(MAKE)` into each, an
// included configuration, .PHONY all and clean, the built-in .c.o rule, $^ and
// $(VAR:.c=.o).  Built, then up to date, then rebuilt where a header moved,
// -n passed down without building, and cleaned.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { APPS, fixture } from "../../cc/test/ccfix.mjs";

const { die, boot } = fixture("make portable");
const { H, put, run, submit } = await boot();
put("/bin/make", new Uint8Array(readFileSync(join(APPS, "build/devel/make/make.wasm"))));

// A file and a time for it: a store write stamps one, a planted file has none.
const file = (path, text) => {
    put(path, text);
    H.store.stamp(path);
};
const exists = (path) => H.store.files.has(path);

const P = "/tmp"; // where ccfix's run() keeps its result files
file(`${P}/config.mk`, "CFLAGS = -DLOUD=1\n");
file(`${P}/Makefile`, `include config.mk
DIRS = lib app

.PHONY: all clean
all:
\tcd lib && $(MAKE)
\tcd app && $(MAKE)

clean:
\tcd lib && $(MAKE) clean
\tcd app && $(MAKE) clean
`);
file(`${P}/lib/Makefile`, `include ../config.mk
SRCS = greet.c shout.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean
all: $(OBJS)
$(OBJS): greet.h

clean:
\t-rm $(OBJS)
`);
file(`${P}/lib/greet.h`, `#include <braam.h>

coro(braam_call *) void greet(const char *who);
const char *shout(void);
`);
file(`${P}/lib/greet.c`, `#include <stdio.h>
#include "greet.h"

coro(braam_call *) void greet(const char *who)
{
    await printf("hello from %s%s\\n", who, shout());
}
`);
file(`${P}/lib/shout.c`, `#include "greet.h"

const char *shout(void)
{
    return LOUD ? "!" : ".";
}
`);
file(`${P}/app/Makefile`, `include ../config.mk
LIBOBJS = ../lib/greet.o ../lib/shout.o

.PHONY: all clean
all: hello
hello: main.o $(LIBOBJS)
\t$(CC) $(LDFLAGS) -o $@ $^
main.o: main.c ../lib/greet.h
\t$(CC) $(CFLAGS) -I../lib -c main.c

clean:
\t-rm hello main.o
`);
file(`${P}/app/main.c`, `#include "greet.h"

coro(braam_call *) int main(int argc, char **argv)
{
    await greet("three directories");
    return 0;
}
`);

// A sub-make says where it is, as GNU's does.
const sub = (dir, ...lines) => [`cd ${dir} && make`,
                               `make[1]: Entering directory '${P}/${dir}'`, ...lines,
                               `make[1]: Leaving directory '${P}/${dir}'`];
const built = [
    ...sub("lib", "cc -DLOUD=1 -c greet.c", "cc -DLOUD=1 -c shout.c"),
    ...sub("app", "cc -DLOUD=1 -I../lib -c main.c",
           "cc  -o hello main.o ../lib/greet.o ../lib/shout.o"),
].join("\n") + "\n";

let r = run("make");
if (r.status !== 0 || r.out !== built)
    die(`the first make: ${JSON.stringify(r)}`);
r = run("app/hello");
if (r.status !== 0 || r.out !== "hello from three directories!\n")
    die(`hello: ${JSON.stringify(r)}`);

// Up to date: only the two sub-makes, the phony targets having no commands.
r = run("make");
if (r.status !== 0 || r.out !== [...sub("lib"), ...sub("app")].join("\n") + "\n")
    die(`the second make: ${JSON.stringify(r)}`);

// The header moves: both library objects, main.o and the program.
run("touch lib/greet.h");
r = run("make -n");
if (r.status !== 0 || r.out !== built)
    die(`make -n after touch greet.h: ${JSON.stringify(r)}`);
if (H.store.mtimes.get(`${P}/lib/greet.o`) > H.store.mtimes.get(`${P}/lib/greet.h`))
    die("make -n built greet.o");
r = run("make");
if (r.status !== 0 || r.out !== built)
    die(`make after touch greet.h: ${JSON.stringify(r)}`);

// A macro on the command line reaches the sub-makes through MAKEFLAGS.
r = run("make clean");
if (r.status !== 0 || exists(`${P}/app/hello`) || exists(`${P}/lib/greet.o`))
    die(`make clean: ${JSON.stringify(r)}`);
r = run("make CFLAGS=-DLOUD=0");
if (r.status !== 0 || !r.out.includes("cc -DLOUD=0 -c shout.c"))
    die(`make CFLAGS=-DLOUD=0: ${JSON.stringify(r)}`);
r = run("app/hello");
if (r.out !== "hello from three directories.\n")
    die(`hello after CFLAGS=-DLOUD=0: ${JSON.stringify(r)}`);

console.log("make portable ok");
