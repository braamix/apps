// Builds libw.a, which the package ships as lib/: wat assembles
// the sources and ar archives them, both run under the SDK's harness.
//
//   node mklib.mjs <wat.wasm> <ar.wasm> <output directory>

import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../wat/host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const [wat, ar, out] = process.argv.slice(2);
if (!out) {
    process.stderr.write("usage: mklib.mjs <wat.wasm> <ar.wasm> <output directory>\n");
    process.exit(1);
}

const inputs = { "mk.sh": "set -e\nwat proc.wat fmt.wat args.wat\nar rc libw.a proc.o fmt.o args.o\n" };
for (const f of ["proc.wat", "fmt.wat", "args.wat"])
    inputs[f] = readFileSync(join(HERE, f));
const r = (await assembler(wat, { ar })).run(["mk.sh"], inputs, null, "sh");
if (r.status !== 0 || r.err || !r.files["libw.a"]) {
    process.stderr.write(`mklib: status ${r.status}\n${r.out}${r.err}`);
    process.exit(1);
}
writeFileSync(join(out, "libw.a"), r.files["libw.a"]);
