// Modules ld refuses to link, which nm and disasm must still read: thread-
// local storage and shared memory, exceptions old and new and the tags they
// throw, -fPIC and a shared library, wasm64, and GC types. Built into `dir`
// by the clang and wasm-ld beside llvm's other tools; returns their names.

import { execFileSync } from "node:child_process";
import { writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

const leb = (n) => {
    const b = [];
    do {
        let x = n & 0x7f;
        n >>>= 7;
        b.push(n ? x | 0x80 : x);
    } while (n);
    return b;
};
const str = (s) => [...leb(s.length), ...new TextEncoder().encode(s)];
const vec = (items) => [...leb(items.length), ...items.flat()];
const section = (id, body) => [id, ...leb(body.length), ...body];
const sub = (id, body) => [id, ...leb(body.length), ...body];

// An object of GC types, the last a function's, and globals, defining one
// function of that last type.
function gc(types, globals) {
    return new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        ...section(1, vec(types)),
        ...section(3, vec([[types.length - 1]])),
        ...(globals.length ? section(6, vec(globals)) : []),
        ...section(10, vec([[2, 0, 0x0b]])),
        ...section(0, [...str("linking"), 2, ...sub(8, vec([[0, 0, 0, ...str("f")]]))])]);
}

export function foreign(m, dir) {
    const bin = dirname(m.objdump);
    const clang = (args) => execFileSync(join(bin, "clang"), args, { cwd: dir });
    const link = (args) => execFileSync(m.wasm_ld, args, { cwd: dir });
    writeFileSync(join(dir, "tls.c"), `_Thread_local int counter = 5;
_Thread_local char buf[16];
static _Thread_local int hidden;
int bump(void) { hidden++; buf[0] = 1; return ++counter; }
`);
    writeFileSync(join(dir, "eh.cpp"), `struct E { int v; };
void thrower(int);
int caught(int x) {
    try { thrower(x); } catch (E &e) { return e.v; } catch (...) { return -1; }
    return 0;
}
void raise(int v) { throw E{v}; }
`);
    writeFileSync(join(dir, "tag.s"), `\t.tagtype\t__cpp_exception i32
\t.globl\t__cpp_exception
__cpp_exception:
\t.tagtype\tlocal_tag i32, i64
local_tag:
\t.globl\tthrower
\t.type\tthrower,@function
thrower:
\t.functype\tthrower (i32) -> ()
\tlocal.get\t0
\tthrow\t__cpp_exception
\tend_function
`);
    const c = ["--target=wasm32", "-O2", "-c"];
    clang([...c, "-pthread", "-matomics", "-mbulk-memory", "tls.c", "-o", "tls.o"]);
    clang([...c, "-fwasm-exceptions", "-mllvm", "-wasm-use-legacy-eh=true", "eh.cpp", "-o", "eh.o"]);
    clang([...c, "-fwasm-exceptions", "-mllvm", "-wasm-use-legacy-eh=false", "eh.cpp", "-o", "ehnew.o"]);
    clang(["--target=wasm32", "-c", "-mexception-handling", "tag.s", "-o", "tag.o"]);
    clang([...c, "-fPIC", "tls.c", "-o", "pic.o"]);
    clang(["--target=wasm64", "-O2", "-c", "-pthread", "-matomics", "-mbulk-memory", "tls.c", "-o", "w64.o"]);
    const lib = ["--no-entry", "--export-all", "--allow-undefined"];
    link(["--no-entry", "--export-all", "--shared-memory", "--import-memory", "--max-memory=131072",
          "tls.o", "-o", "tls.wasm"]);
    link([...lib, "eh.o", "tag.o", "-o", "eh.wasm"]);
    link([...lib, "ehnew.o", "tag.o", "-o", "ehnew.wasm"]);
    link(["-shared", "--experimental-pic", "pic.o", "-o", "pic.so"]);
    link([...lib, "-mwasm64", "--shared-memory", "--max-memory=131072", "w64.o", "-o", "w64.wasm"]);
    const gcs = [
        [[[0x4e, 1, 0x60, 0, 0]], []],
        [[[0x4e, 2, 0x5f, 1, 0x7f, 1, 0x60, 0, 0]], []],
        [[[0x5f, 1, 0x7f, 1], [0x60, 0, 0]], []],
        [[[0x5e, 0x78, 1], [0x60, 0, 0]], []],
        [[[0x60, 1, 0x64, 0x70, 0]], []],
        [[[0x60, 0, 0]], [[0x63, 0, 0, 0xd0, 0x70, 0x0b]]],
    ];
    gcs.forEach(([t, g], i) => writeFileSync(join(dir, `gc${i}.o`), gc(t, g)));
    return ["tls.o", "eh.o", "ehnew.o", "tag.o", "pic.o", "w64.o", ...gcs.map((_, i) => `gc${i}.o`),
            "tls.wasm", "eh.wasm", "ehnew.wasm", "pic.so", "w64.wasm"];
}
