// vcc's run tests, compiled by cc on Braam and run there, each output held to
// the string upstream's test holds it to (test/programs/<suite>/<Test>.expect).
// A program with no .expect is checked by a test of its own upstream, and is
// not run here.
//
// Upstream runs these as plain wasm32 programs, whose main is an ordinary
// function. On Braam main is a coroutine, so each program is compiled with
// -Dmain=__vcc_main and linked with a main that calls it; for the book's
// suites that main prints the result after the output, as upstream's
// crt0-status.o does. A program that prints has had its main made a coroutine
// (printf being one on Braam), and is linked with a main that awaits it.

import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { TREE, fixture } from "./ccfix.mjs";

const MAIN = `#include <stdio.h>
#if CORO
coro(braam_call *) int __vcc_main(void);
#else
int __vcc_main(void);
#endif
coro(braam_call *) int main(int argc, char **argv)
{
#if CORO
    int s = await __vcc_main();
#else
    int s = __vcc_main();
#endif
#if STATUS
    await printf("%d\\n", s);
#endif
    return s;
}
`;

// Whether a program's main is a coroutine.
const coro_main = (src) => /coro\(braam_call \*\)\s+int\s+main\b/.test(src);

// name: the test's; suites: directories of test/programs; status: whether
// main's result is printed; skip: {program: why} for those that cannot run here.
export async function run_suites(name, suites, status, skip) {
    const { die, boot } = fixture(name);
    const session = await boot();
    console.log(check_suites(session, die, name, suites, status, skip));
}

// The same in a session already booted: what it ran, or die() saying what
// differs.
export function check_suites({ put, text, sh }, die, name, suites, status, skip) {
    put("/tmp/main.c", MAIN);
    for (const coro of [0, 1]) {
        const r = sh(`cc -c -DSTATUS=${status ? 1 : 0} -DCORO=${coro} main.c -o main${coro}.o`);
        if (r.status !== 0)
            die(`main.c: ${r.err}`);
    }

    const programs = [];
    const expected = new Set();
    let unchecked = 0;
    for (const suite of suites) {
        const dir = join(TREE, "test/programs", suite);
        for (const f of readdirSync(dir).filter((f) => f.endsWith(".c")).sort()) {
            const n = f.slice(0, -2);
            let expect;
            try {
                expect = readFileSync(join(dir, n + ".expect"), "utf8");
            } catch {
                unchecked++;
                continue;
            }
            expected.add(n);
            if (n in skip)
                continue;
            const src = readFileSync(join(dir, f));
            put(`/tmp/${n}.c`, new Uint8Array(src));
            programs.push({ n, expect, coro: coro_main(src.toString()) ? 1 : 0 });
        }
    }
    for (const n of Object.keys(skip))
        if (!expected.has(n))
            die(`${n} is skipped, but there is no such program with an .expect`);

    let script = "";
    for (const { n, coro } of programs)
        script += `cc -Dmain=__vcc_main ${n}.c main${coro}.o -o ${n} 2>${n}.e && ./${n} >${n}.o 2>&1\n`;
    sh(script);

    const bad = [];
    for (const { n, expect } of programs) {
        const out = text(`/tmp/${n}.o`);
        if (out === expect)
            continue;
        const err = text(`/tmp/${n}.e`);
        bad.push(`${n}: ${err ? err.split("\n")[0] : JSON.stringify(out) + ", expected " +
                                                         JSON.stringify(expect)}`);
    }
    if (bad.length)
        die(`${bad.length} of ${programs.length} differ:\n` + bad.join("\n"));
    return `${name} ok: ${programs.length} programs as upstream expects, ` +
           `${Object.keys(skip).length} skipped, ${unchecked} with no expectation`;
}
