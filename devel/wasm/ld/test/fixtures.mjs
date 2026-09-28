// The fixtures and the oracle, independent of the linker. Every input is
// what it claims to be, and wasm-ld's link of every fixture passes the checks
// ld's links will be held to — which proves the checks, not wasm-ld.

import { readFileSync } from "node:fs";
import { boot, check_archive, check_object, check_run, check_surface, die, fixtures, ok }
    from "./wasmlib.mjs";

const all = fixtures();
const H = await boot();

const failures = [];
for (const [name, fx] of Object.entries(all)) {
    const bad = [];
    for (const path of fx.objects) {
        const why = check_object(path);
        if (why)
            bad.push(why);
    }
    for (const path of [...fx.archives, ...fx.libs]) {
        const why = check_archive(path);
        if (why)
            bad.push(why);
    }
    const bytes = new Uint8Array(readFileSync(fx.reference));
    bad.push(...check_surface(name, bytes));
    bad.push(...check_run(H, name, bytes));
    for (const b of bad)
        failures.push(`${name}: ${b}`);
}

if (failures.length)
    die("wasm-ld's fixtures fail the oracle:\n" + failures.join("\n"));
ok(`${Object.keys(all).length} fixtures under wasm-ld`);
