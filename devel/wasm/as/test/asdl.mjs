// wat.asdl, checked by validate_asdl.py: it must parse, every field's type
// must be defined, no constructor twice, every type reachable from Module.
// Then broken copies of it, each of which the validator must refuse, so a
// check that stops working fails here too. Needs python3 and pyasdl.

import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const DIR = join(dirname(fileURLToPath(import.meta.url)), "..");
const ASDL = join(DIR, "wat.asdl");
const PRIMITIVES = "name,u8,u32,u64,opcode,location";
const tmp = mkdtempSync(join(tmpdir(), "asdl-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));

function validate(file) {
    return spawnSync("python3", [join(DIR, "validate_asdl.py"), "-p", PRIMITIVES, file],
                     { encoding: "utf8" });
}

let bad = 0;
function fail(msg) {
    console.error("asdl: " + msg);
    bad++;
}

const good = validate(ASDL);
if (good.error)
    fail(`cannot run python3: ${good.error.message}`);
else if (good.status !== 0)
    fail(`wat.asdl is refused:\n${good.stdout}${good.stderr}`);
else
    process.stdout.write(good.stdout);

// Each case changes one thing, and the validator must name it.
const text = readFileSync(ASDL, "utf8");
const cases = [
    ["undefined type", "Param* params, ValType* results", "Parm* params, ValType* results",
     "undefined type 'Parm'"],
    ["undefined primitive", "u32 count", "u31 count", "undefined type 'u31'"],
    ["syntax error", "Limits = (u64 min,", "Limits = (u64 min,,", "syntax error"],
    ["duplicate constructor", "| Likely ", "| Unlikely ", "constructor 'Unlikely'"],
    ["duplicate type", "    Expr = (Instr* instrs)",
     "    Expr = (Instr* instrs)\n    Expr = (Instr* more)", "type 'Expr' defined twice"],
    ["unreachable type", "BranchHint? hint)\n", "BranchHint? hint)\n    Lost = Lost\n",
     "type 'Lost' is not reachable"],
];
for (const [what, from, to, expect] of cases) {
    if (!text.includes(from)) {
        fail(`${what}: wat.asdl has no '${from.trim()}' to change`);
        continue;
    }
    const file = join(tmp, what.replace(/ /g, "-") + ".asdl");
    writeFileSync(file, text.replace(from, to));
    const r = validate(file);
    if (r.status === 0)
        fail(`${what}: accepted`);
    else if (!r.stderr.includes(expect))
        fail(`${what}: says\n${r.stderr}not '${expect}'`);
}

if (bad)
    process.exit(1);
console.log(`asdl: ok, and ${cases.length} broken copies refused`);
