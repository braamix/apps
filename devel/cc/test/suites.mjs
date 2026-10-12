// vcc's run tests, compiled by cc on Braam and run there, each output held to
// the string upstream's test holds it to (test/programs/<suite>/<Test>.expect).
// A program with no .expect is checked by a test of its own upstream, and is
// not run here.
//
// Upstream runs these as plain wasm32 programs, whose main is an ordinary
// function. On Braam main is a coroutine, so each program is compiled with
// -Dmain=__vcc_main and linked with a main that calls it; for the book's
// suites that main prints the result after the output, as upstream's
// crt0-status.o does.

import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { TREE, fixture } from "./ccfix.mjs";

const MAIN = `#include <stdio.h>
int __vcc_main(void);
coro(braam_call *) int main(int argc, char **argv)
{
    int s = __vcc_main();
#if STATUS
    printf("%d\\n", s);
#endif
    return s;
}
`;

// name: the test's; suites: directories of test/programs; status: whether
// main's result is printed; skip: {program: why} for those that cannot run here.
export async function run_suites(name, suites, status, skip) {
    const { die, boot } = fixture(name);
    const { put, text, sh } = await boot();
    put("/tmp/main.c", MAIN);
    let r = sh(`cc -c -DSTATUS=${status ? 1 : 0} main.c`);
    if (r.status !== 0)
        die(`main.c: ${r.err}`);

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
            put(`/tmp/${n}.c`, new Uint8Array(readFileSync(join(dir, f))));
            programs.push({ n, expect });
        }
    }
    for (const n of Object.keys(skip))
        if (!expected.has(n))
            die(`${n} is skipped, but there is no such program with an .expect`);

    let script = "";
    for (const { n } of programs)
        script += `cc -Dmain=__vcc_main ${n}.c main.o -o ${n} 2>${n}.e && ./${n} >${n}.o 2>&1\n`;
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
    console.log(`${name} ok: ${programs.length} programs as upstream expects, ` +
                `${Object.keys(skip).length} skipped, ${unchecked} with no expectation`);
}
