// Layout. For every fixture, wlink's --dump-layout must equal what wasm-ld
// built: its types, imports, globals, table and elements, read from its
// output, and its memory map, read from -Map at -O0 (wlink merges no strings
// yet). wasm-ld's __wasm_init_memory is left out: wlink writes active
// segments and has none. The --verbose "mem:" lines must be wasm-ld's, and
// memory that is too small must be refused in wasm-ld's words.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { boot, die, FLAGS, index_spaces, linkers, manifest, map_layout, ok } from "./wlinklib.mjs";

const m = manifest();
const H = await boot();
const { tmp, inputs, wasm_ld, wlink } = linkers(H, m);

const bad = [];
const same = (what, a, b) => {
    if (a !== b)
        bad.push(`${what}:\n  wlink:   ${JSON.stringify(a)}\n  wasm-ld: ${JSON.stringify(b)}`);
};
const INIT_MEMORY = "<internal>:(__wasm_init_memory)";
const mem = (err, who) => err.split("\n").filter((l) => l.startsWith(`${who}: mem: `))
    .map((l) => l.slice(who.length)).join("\n");

function compare(what, fx, extra = []) {
    const args = [...FLAGS, ...extra, ...inputs(fx)];
    const want = wasm_ld([...args, "-O0", "--verbose", "-Map=map"]);
    if (want.status !== 0)
        die(`wasm-ld fails on ${what}: ${want.err}`);
    const bytes = new Uint8Array(readFileSync(join(tmp, "out.wasm")));
    const expected = [...index_spaces(bytes),
        ...map_layout(readFileSync(join(tmp, "map"), "utf8"), [INIT_MEMORY])];
    const got = wlink([...args, "--verbose", "--dump-layout"]);
    same(`${what}: status`, got.status, 0);
    same(`${what}: mem`, mem(got.err, "wlink"), mem(want.err, "wasm-ld"));
    const lines = got.out.split("\n").filter(Boolean);
    for (let i = 0; i < Math.max(lines.length, expected.length); i++)
        if (lines[i] !== expected[i]) {
            same(`${what}: layout line ${i + 1}`, lines[i], expected[i]);
            break;
        }
    return expected.length;
}

let lines = 0;
for (const [name, fx] of Object.entries(m.fixtures))
    lines += compare(name, fx);
compare("--no-stack-first", m.fixtures.data, ["--no-stack-first"]);
compare("--global-base", m.fixtures.data, ["--no-stack-first", "--global-base=4096"]);
compare("--max-memory", m.fixtures.hello, ["--max-memory=2097152"]);
// Alone, layout_a.c's data ends 5 bytes past a 16-byte boundary. Exporting
// __wasm_call_ctors keeps wasm-ld from wrapping exports, as Braam's _start does.
const layout_a = m.fixtures.layout.objects.filter((o) => o.endsWith("layout_a.c.obj"));
compare("heap alignment", { objects: layout_a, libs: [] },
        ["--allow-undefined", "--export=fx_main", "--export=__wasm_call_ctors"]);

const as_wlink = (s) => s.replaceAll("wasm-ld: ", "wlink: ");
function refuse(what, fx, extra) {
    const args = [...FLAGS.filter((f) => !f.startsWith("--initial-memory")), ...extra, ...inputs(fx)];
    const want = wasm_ld(args);
    const got = wlink(args);
    same(`${what}: status`, got.status, 1);
    same(`${what}: stderr`, got.err, as_wlink(want.err));
}
refuse("initial memory", m.fixtures.data, ["--initial-memory=131072"]);
refuse("initial alignment", m.fixtures.data, ["--initial-memory=1000000"]);
refuse("stack alignment", m.fixtures.hello, ["-z", "stack-size=1000"]);
refuse("maximum memory", m.fixtures.data, ["--max-memory=131072"]);
refuse("global base", m.fixtures.hello, ["--global-base=1024"]);

if (bad.length)
    die("\n" + bad.join("\n"));
ok(`${Object.keys(m.fixtures).length} fixtures lay out as wasm-ld lays them out ` +
   `(${lines} lines); 5 refusals in its words`);
