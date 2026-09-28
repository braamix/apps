// Liveness. For every fixture, ld's --print-gc-sections must name what
// wasm-ld's names, in its order: every function, segment and global either
// linker drops. Undefined symbols only dead code reaches are not errors, and
// without --gc-sections nothing is dropped.

import { boot, die, FLAGS, linkers, manifest, ok } from "./wasmlib.mjs";

const m = manifest();
const H = await boot();
const { inputs, wasm_ld, ld } = linkers(H, m);

const bad = [];
const same = (what, a, b) => {
    if (a !== b)
        bad.push(`${what}:\n  ld:      ${JSON.stringify(a)}\n  wasm-ld: ${JSON.stringify(b)}`);
};

let dropped = 0;
for (const [name, fx] of Object.entries(m.fixtures)) {
    const args = [...FLAGS, "--print-gc-sections", ...inputs(fx)];
    const want = wasm_ld(args);
    if (want.status !== 0)
        die(`wasm-ld fails on ${name}: ${want.err}`);
    const got = ld(args);
    same(`${name}: status`, got.status, 0);
    same(`${name}: stderr`, got.err, want.err.replaceAll("wasm-ld: ", "ld: "));
    same(`${name}: --print-gc-sections`, got.out, want.out);
    dropped += want.out.split("\n").length - 1;
}

// Without the runtime nothing calls __wasm_call_ctors, and a live init
// function keeps it all the same.
const ctors = m.fixtures.ctors.objects.filter((o) => o.endsWith("ctors.cpp.obj"));
const bare = [...FLAGS, "--print-gc-sections", "--allow-undefined",
    ...inputs({ objects: ctors, libs: [] })];
const want = wasm_ld(bare);
same("no runtime: wasm-ld", want.status, 0);
same("no runtime: --print-gc-sections", ld(bare).out, want.out);

const all = [...FLAGS.filter((f) => f !== "--gc-sections"), "--no-gc-sections",
    "--print-gc-sections", "--allow-undefined", ...inputs(m.fixtures.live)];
const got = ld(all);
same("--no-gc-sections: status", got.status, 0);
same("--no-gc-sections: --print-gc-sections", got.out, "");

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`${Object.keys(m.fixtures).length} fixtures drop what wasm-ld drops (${dropped} chunks)`);
