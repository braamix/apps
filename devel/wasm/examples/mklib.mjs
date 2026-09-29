// Builds crt.o and libw.a, which the package ships as lib/: as assembles
// the sources and ar archives them, both run under the SDK's harness.
//
//   node mklib.mjs <as.wasm> <ar.wasm> <output directory>

import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../as/host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const [as, ar, out] = process.argv.slice(2);
if (!out) {
    process.stderr.write("usage: mklib.mjs <as.wasm> <ar.wasm> <output directory>\n");
    process.exit(1);
}

const inputs = { "mk.sh": "set -e\nas crt.s proc.s fmt.s args.s\nar rc libw.a proc.o fmt.o args.o\n" };
for (const f of ["crt.s", "proc.s", "fmt.s", "args.s"])
    inputs[f] = readFileSync(join(HERE, f));
const r = (await assembler(as, { ar })).run(["mk.sh"], inputs, null, "sh");
if (r.status !== 0 || r.err || !r.files["crt.o"] || !r.files["libw.a"]) {
    process.stderr.write(`mklib: status ${r.status}\n${r.out}${r.err}`);
    process.exit(1);
}
for (const f of ["crt.o", "libw.a"])
    writeFileSync(join(out, f), r.files[f]);
