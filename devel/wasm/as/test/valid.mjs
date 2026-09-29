// as's validation, where the test suite does not reach: the instructions of
// §10 of the language. Each valid module must be assembled and pass
// wabt's wasm-validate too; each invalid one must be refused, with its
// message and place. The suite's own assert_invalid modules are spec.mjs's.

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");

function die(msg) {
    console.error("valid: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
const as = await assembler(AS);
const tmp = mkdtempSync(join(tmpdir(), "as-valid-"));
process.on("exit", () => rmSync(tmp, { recursive: true, force: true }));
const bad = [];

const V = "(v128.const i32x4 0 0 0 0)";
const VALID = [
    ["threads", `(memory 1 1 shared) (func atomic.fence
  (i32.atomic.store (i32.const 0) (i32.const 1))
  (drop (i64.atomic.rmw.add (i32.const 8) (i64.const 1)))
  (drop (i32.atomic.rmw8.cmpxchg_u (i32.const 0) (i32.const 1) (i32.const 2)))
  (drop (i64.atomic.load32_u (i32.const 8)))
  (drop (memory.atomic.notify (i32.const 0) (i32.const 1)))
  (drop (memory.atomic.wait32 (i32.const 0) (i32.const 0) (i64.const -1)))
  (drop (memory.atomic.wait64 (i32.const 8) (i64.const 0) (i64.const -1))))`],
    ["wide arithmetic", `(func (result i64 i64)
  (drop (drop (i64.add128 (i64.const 1) (i64.const 2) (i64.const 3) (i64.const 4))))
  (i64.mul_wide_u (i64.const 1) (i64.const 2)))`],
    ["relaxed SIMD", `(func (result v128)
  (drop (f32x4.relaxed_madd ${V} ${V} ${V}))
  (drop (i32x4.relaxed_dot_i8x16_i7x16_add_s ${V} ${V} ${V}))
  (drop (i16x8.relaxed_dot_i8x16_i7x16_s ${V} ${V}))
  (i32x4.relaxed_trunc_f64x2_u_zero ${V}))`],
    ["custom page sizes", `(memory $p 0 10 (pagesize 1)) (memory i64 1 (pagesize 65536))
(func (result i32) (i32.load8_u $p (i32.const 3)))`],
    ["legacy exceptions", `(tag $e (param i32)) (func (param i32) (result i32)
  try (result i32) (throw $e (local.get 0))
  catch $e try rethrow 1 delegate 0 unreachable
  catch_all i32.const 0 end)`],
];

// [source, line:col: message]
const INVALID = [
    ["(memory 1 1 shared) (func (drop (i32.atomic.load align=2 (i32.const 0))))",
     "1:34: atomic alignment must be natural"],
    ["(memory 1 shared)", "1:2: shared memory must have maximum"],
    ["(func (drop (i64.add128 (i64.const 1) (i64.const 2))))",
     "1:14: type mismatch: instruction requires [i64 i64 i64 i64] but stack has [i64 i64]"],
    ["(func (result i64) (i64.mul_wide_s (i64.const 1) (i64.const 2)))",
     "1:2: type mismatch: block requires [i64] but stack has [i64 i64]"],
    ["(func block rethrow 0 end)", "1:13: invalid rethrow label"],
    ["(func try delegate 1)", "1:7: unknown label 1"],
    ["(func try catch 0 end)", "1:7: unknown tag 0"],
    ["(memory 1 (pagesize 2))", "1:2: invalid custom page size"],
    [`(func (drop (f32x4.relaxed_madd ${V} ${V})))`,
     "1:14: type mismatch: instruction requires [v128 v128 v128] but stack has [v128 v128]"],
];

{
    const inputs = Object.fromEntries(VALID.map(([, src], k) => [`v${k}.s`, src]));
    const r = as.run(["--module", ...Object.keys(inputs)], inputs);
    VALID.forEach(([what], k) => {
        const b = r.files[`v${k}.wasm`];
        if (!b)
            return bad.push(`${what}: refused: ${r.err}`);
        writeFileSync(join(tmp, "v.wasm"), b);
        const w = spawnSync("wasm-validate", ["--enable-all", join(tmp, "v.wasm")], { encoding: "utf8" });
        if (w.error)
            die("wants wabt's wasm-validate");
        if (w.status !== 0)
            bad.push(`${what}: wasm-validate refuses: ${w.stderr}`);
    });
}
{
    const inputs = Object.fromEntries(INVALID.map(([src], k) => [`e${k}.s`, src]));
    const r = as.run(["--module", ...Object.keys(inputs)], inputs);
    const said = new Map();
    for (const l of r.err.split("\n").filter((l) => l))
        said.set(l.slice(0, l.indexOf(":")), l.slice(l.indexOf(":") + 1));
    INVALID.forEach(([src, want], k) => {
        const got = said.get(`e${k}.s`);
        const expect = want.replace(/^(\d+:\d+): /, "$1: error: ");
        if (got !== expect)
            bad.push(`${JSON.stringify(src)}: ${got ?? "accepted"}, expected ${expect}`);
    });
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.join("\n  "));
console.log(`valid ok: ${VALID.length} valid modules, as wasm-validate agrees; ${INVALID.length} invalid refused`);
