// as's parser. First `as --tree` of crafted modules, one for each
// abbreviation the parser expands and each ambiguity it settles, held to
// a golden file (BLESS=1 writes it). Then crafted errors, each with its
// message and place, and blocks and folded instructions nested 10000 deep.
// test/resolve.mjs runs the test suite.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");

function die(msg) {
    console.error("parser: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
const as = await assembler(AS);
const bad = [];

// ------------------------------------------------------------ golden trees

const TREES = [
    ["first look", `(module
  (import "env" "puts" (func $puts (param i32)))
  (memory (export "memory") 1)
  (data (i32.const 16) "hello\\00")
  (func $main (export "main") (result i32)
    (local $i i32)
    (call $puts (i32.const 16))
    (local.set $i (i32.const 3))
    (block $done
      (loop $again
        (br_if $done (i32.eqz (local.get $i)))
        local.get $i
        i32.const 1
        i32.sub
        local.set $i
        br $again))
    i32.const 0))`],
    ["bare fields", `(func) (memory 0)`],
    ["empty", ``],
    ["module id and name", `(module $m (@name "Modül"))`],
    ["folded unfolded", `(func (i32.mul (i32.add (local.get 0) (i32.const 2)) (i32.const 3)) drop)`],
    ["grouped split", `(type (func (param i32 i64) (param) (param $x f32) (result i32 i64) (result)))
(type (struct (field i32 (mut i64)) (field $f i8) (field)))
(func (param i32 i32) (local f32 f64) (local $l v128) (local))`],
    ["type without sub", `(type $a (func)) (type $b (sub (func))) (type $c (sub final $b (func)))
(type (array (mut i16)))`],
    ["rec", `(rec (type $a (struct (field (ref null $b)))) (type $b (array (ref $a)))) (rec)`],
    ["ref types", `(func (param anyref eqref i31ref structref arrayref nullref funcref nullfuncref)
  (param exnref nullexnref externref nullexternref (ref 0) (ref null $t) (ref func) (ref null extern)))`],
    ["if without else", `(func
  i32.const 0 if nop end
  (if (i32.const 1) (then nop))
  i32.const 2 if $l (result i32) i32.const 3 else $l i32.const 4 end $l drop
  (if (result i32) (i32.const 5) (then (i32.const 6)) (else (i32.const 7))) drop)`],
    ["block types", `(func block end block (result i32) unreachable end
  block (param i32) drop end block (result i32 i64) unreachable end
  block (type 0) end block (type 0) (param i32) (result i32) end
  (loop $l (result f32) unreachable) (block (param i32) (result i32)))`],
    ["labels", `(func block $a block $b br $a br 1 end $b end $a)`],
    ["indices left out", `(table 1 funcref) (memory 1) (type (func))
(func table.get table.set table.size table.grow table.fill table.copy
  memory.size memory.grow memory.fill memory.copy
  call_indirect (type 0) return_call_indirect (type 0)
  table.get 1 table.copy 2 3 memory.size $m memory.copy $a $b)`],
    ["init x? y", `(func table.init 1 table.init 2 3 memory.init 4 memory.init $m $d
  table.init $e elem.drop 5 data.drop $d)`],
    ["memory operands", `(func i32.load i64.load offset=8 i32.load align=1 i32.load8_u offset=0x10 align=1
  v128.load i32.load 1 i64.store $m offset=4 align=8 f64.load 0 offset=1
  i64.atomic.rmw32.cmpxchg_u offset=4 memory.atomic.wait64)`],
    ["lane accesses", `(func v128.load8_lane 1 v128.load8_lane 1 2 v128.load16_lane $m 3
  v128.load32_lane offset=4 5 v128.store64_lane align=8 1 v128.load8_lane 0 offset=1 align=1 15
  i8x16.extract_lane_s 15 f64x2.replace_lane 1)`],
    ["select", `(func select (select (local.get 0) (local.get 1) (local.get 2))
  select (result i32) (select (result i32) (result) (local.get 0) (local.get 1) (local.get 2))
  select (result))`],
    ["call_indirect", `(func call_indirect $t (type 0) (param i32) (result i32)
  (call_indirect $t (type $f) (param i32) (result i32) (local.get 0) (local.get 1))
  (call_indirect (param i64) (i64.const 1) (i32.const 0))
  (return_call_indirect 2 (type 1)))`],
    ["constants", `(func i32.const -1 i32.const 0xffff_ffff i64.const -9223372036854775808
  f32.const nan:0x1 f32.const -0 f64.const 0x1p-1074 f64.const -inf f32.const 1e38
  v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 -1
  v128.const i16x8 0 1 2 3 4 5 6 -1 v128.const i32x4 1 2 3 -1
  v128.const i64x2 1 -1 v128.const f32x4 1 -0 nan inf v128.const f64x2 0.5 -inf
  i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 31)`],
    ["branches", `(func block br_table 0 0 0 br_table 0 br_on_null 0 br_on_non_null 0
  br_on_cast 0 anyref (ref eq) br_on_cast_fail 0 (ref null any) (ref $t) end
  return call 0 return_call $f call_ref $t return_call_ref 0 throw $e throw_ref)`],
    ["references and aggregates", `(func ref.null func ref.null $t ref.null none ref.func $f
  ref.is_null ref.as_non_null ref.eq ref.test (ref $t) ref.cast (ref null i31) ref.i31
  i31.get_s struct.new $s struct.new_default 0 struct.get $s $f struct.get_s 0 1
  struct.set $s 0 array.new $a array.new_fixed $a 3 array.new_data $a $d
  array.new_elem 0 1 array.get $a array.set 0 array.len array.copy $a $b array.fill $a
  array.init_data $a 0 array.init_elem $a $e any.convert_extern extern.convert_any)`],
    ["variables", `(global $g (mut i32) (i32.const 0)) (global i64 i64.const 1)
(func (param $p i32) local.get $p local.set 0 local.tee $p global.get $g global.set 0)`],
    ["try_table", `(tag $e (param i32)) (func
  block $h (result exnref) try_table (catch $e 0) (catch_ref 0 0) (catch_all 0) (catch_all_ref $h) end
  (try_table $t (result i32) (catch $e 0) (i32.const 1)) unreachable end drop)`],
    ["legacy try", `(tag $e) (func
  try $t catch $e rethrow $t catch_all nop end $t
  try nop catch_all end
  try delegate 0
  (try (result i32) (do (i32.const 1)) (catch $e (i32.const 2)) (catch_all (i32.const 3))) drop
  (try (do) (delegate 0)) (try (do)))`],
    ["tables", `(table 1 funcref) (table $t i64 2 10 (ref null func) (ref.null func))
(table 0 externref ref.null extern) (table 1 (ref func) (ref.func $f))`],
    ["table inline elements", `(table $t funcref (elem $f $g 1)) (table i64 externref (elem))
(table (ref null func) (elem (ref.func $f) (item ref.null func) (item)))`],
    ["memories", `(memory 1) (memory $m i64 2 3) (memory 1 2 shared) (memory 0 (pagesize 1))`],
    ["memory inline data", `(memory $m (data "hi" "there")) (memory i64 (data))
(memory (pagesize 1) (data "xyz")) (memory (data "\\00\\01\\02"))`],
    ["inline exports", `(func $f (export "a") (export "b")) (global (export "g") i32 (i32.const 0))
(table (export "t") 0 funcref) (memory (export "m") 0) (tag (export "e"))`],
    ["inline imports", `(func $f (export "e") (import "m" "f") (param i32) (result i32))
(table (import "m" "t") 1 funcref) (memory $m (import "m" "mem") i64 1 2)
(global $g (export "x") (export "y") (import "m" "g") (mut f32)) (tag (import "m" "e") (param i32))
(func (import "m" "f2") (type 0))`],
    ["imports", `(import "m" "f" (func $f (param i32))) (import "m" "t" (table 1 funcref))
(import "m" "mem" (memory 1 shared)) (import "m" "g" (global i32))
(import "m" "e" (tag $e (type 0))) (import "\\u{1F600}" "" (func (type $t) (param i32)))`],
    ["exports and start", `(export "f" (func 0)) (export "t" (table $t)) (export "m" (memory 0))
(export "g" (global $g)) (export "e" (tag 0)) (start $main)`],
    ["elem segments", `(elem (i32.const 0) $f $g) (elem (i32.const 1)) (elem $e (table $t) (offset i32.const 2) func 0)
(elem (offset (i32.const 3)) funcref (ref.func 0) (item ref.null func))
(elem func $f) (elem externref) (elem declare func $f 1) (elem (i32.const 4) func)`],
    ["data segments", `(data "a" "b") (data $d (i32.const 0) "c") (data (memory $m) (offset i32.const 1))
(data (memory 1) (i64.const 2) "d") (data $e)`],
    ["quoted ids", `(func $"a b" (param $"\\u{3bb}" i32) block $"x y" br $"x y" end call $"a b")`],
    ["annotations", `(@custom "c0") (module (@name "M") (@unknown x (y) "z")
  (@custom "c1" (before first) "a" "b") (func $f (@name "λ") (param $x (@name "α") i32) (local (@name "l") i32)
    (@metadata.code.branch_hint "\\01") (if (local.get 0) (then))
    local.get 0 (@metadata.code.branch_hint "\\00") br_if 0
    local.get 0 (@metadata.code.branch_hint "\\00") if end)
  (@custom "c2" (after code)) (@custom "c3" (after last) "x") (@custom "c4" (before datacount)))`],
    ["data annotations", `(memory 1) (data $a (@sym rodata align=4) "hi" (@reloc $a 2) "\\00")
  (data $b (@sym) (i32.const 64) "x") (data $c (@sym bss) "\\00")
  (func i32.const (@reloc $a) i32.load (@reloc $b -1) align=1 drop
    (drop (i64.const (@reloc $b 0x10))))`],
    ["extensions", `(memory 1 1 shared) (func atomic.fence i32.atomic.load i64.atomic.rmw.add offset=8
  memory.atomic.notify i64.add128 i64.sub128 i64.mul_wide_s i64.mul_wide_u
  i8x16.relaxed_swizzle f32x4.relaxed_madd i32x4.relaxed_dot_i8x16_i7x16_add_s)`],
];

{
    const golden = join(HERE, "parser.golden");
    const inputs = Object.fromEntries(TREES.map(([, src], k) => [`t${k}.wat`, src]));
    const r = as.run(["--tree", ...Object.keys(inputs)], inputs);
    if (r.status !== 0 || r.err)
        bad.push(`--tree: status ${r.status}: ${r.err}`);
    const trees = r.out.split(/^(?=\(Module )/m);
    if (trees.length !== TREES.length)
        bad.push(`--tree: ${trees.length} trees for ${TREES.length} modules`);
    const text = TREES.map(([name, src], k) => `=== ${name}\n${src}\n---\n${trees[k] ?? ""}`).join("\n");
    if (process.env.BLESS)
        writeFileSync(golden, text);
    else if (text !== readFileSync(golden, "utf8")) {
        const x = text.split("\n"), y = readFileSync(golden, "utf8").split("\n");
        const i = x.findIndex((l, k) => l !== y[k]);
        bad.push(`--tree: line ${i + 1} is ${JSON.stringify(x[i])}, golden ${JSON.stringify(y[i])}`);
    }
}

// ------------------------------------------------------------ crafted errors

// [source, line:col: message]
const ERRORS = [
    ["(func (i32.add (i32.const 1) i32.const 2))", "1:30: unexpected token"],
    ["(func block end $l)", "1:17: mismatching label"],
    ["(func block $a end $b)", "1:20: mismatching label"],
    ["(func if $a else $b end)", "1:18: mismatching label"],
    ["(func i32.load align=3)", "1:16: alignment must be a power of two"],
    ["(func i32.load align=0)", "1:16: alignment must be a power of two"],
    ["(func i32.load offset=18446744073709551616)", "1:16: i64 constant out of range"],
    ["(func br 4294967296)", "1:10: i32 constant out of range"],
    ["(memory 18446744073709551616)", "1:9: i64 constant out of range"],
    ["(func i8x16.extract_lane_s 256)", "1:28: i8 constant out of range"],
    ["(func v128.const i32x4 1 2 3)", "1:7: wrong number of lane literals"],
    ["(func v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 256)", "1:54: constant out of range"],
    ["(func v128.const i32x4 1 2 3 4.5)", "1:30: constant out of range"],
    ["(func v128.const i32x5 1)", "1:18: unknown operator i32x5"],
    ["(func i8x16.shuffle 1 2)", "1:7: wrong number of lane indices"],
    ["(func i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15.0)", "1:56: unexpected token"],
    ["(func i32.const 1.5)", "1:17: constant out of range"],
    ["(func i64.const 9223372036854775808_0)", "1:17: constant out of range"],
    ["(func f32.const 1e39)", "1:17: constant out of range"],
    ["(func i32.const $x)", "1:17: unexpected token"],
    ["(memory 1) (import \"a\" \"b\" (func))", "1:12: import after memory definition"],
    ["(func) (global i32 i32.const 0) (import \"a\" \"b\" (func)) (import \"a\" \"c\" (func))",
     "1:33: import after global definition"],
    ["(func (import \"a\" \"b\")) (table 0 funcref) (tag (import \"a\" \"c\"))",
     "1:43: import after table definition"],
    ["(start 0) (start 0)", "1:11: multiple start sections"],
    ["(func get_local 0)", "1:7: unknown operator get_local"],
    ["(func i32.wrap/i64)", "1:7: unknown operator i32.wrap/i64"],
    ["(func block", "1:12: unexpected token"],
    ["(module (func) x)", "1:16: unknown operator x"],
    ["(module (func)) (func)", "1:17: unexpected token"],
    ["(module (module))", "1:10: unexpected token"],
    ["(func block (param $y i32) end)", "1:20: unexpected token"],
    ["(func call_indirect (param $y i32))", "1:28: unexpected token"],
    ["(func (if (i32.const 1)))", "1:24: unexpected token"],
    ["(func (if (i32.const 1) (then) (then)))", "1:33: unexpected token"],
    ["(func (block nop) end)", "1:19: unexpected token"],
    ["(func try catch_all catch 0 end)", "1:21: unexpected token"],
    ["(func try catch 0 delegate 0)", "1:19: unexpected token"],
    ["(func (try (catch 0)))", "1:13: unexpected token"],
    ["(func (param i32) (result i32) (param i64))", "1:33: unexpected token"],
    ["(func (local i32) (param i32))", "1:20: unexpected token"],
    ["(func nop (local i32))", "1:12: unexpected token"],
    ["(func (export \"\\ff\"))", "1:15: malformed UTF-8 encoding"],
    ["(memory f32 1)", "1:9: malformed address type"],
    ["(table 1 (elem))", "1:11: unexpected token"],
    ["(elem (table 0) (i32.const 0) 0)", "1:31: unexpected token"],
    ["(type (func (result i32) (param i32)))", "1:26: unexpected token"],
    ["(func ref.null)", "1:15: unexpected token"],
    ["(func br_table)", "1:15: unexpected token"],
    ["(func i32.const +1 br +1)", "1:23: unexpected token"],
    ["(@custom \"x\" (before foo))", "1:14: @custom annotation: malformed section kind"],
    ["(@custom 1)", "1:1: @custom annotation: missing section name"],
    ["(func (@name 1))", "1:14: @name annotation: string expected"],
    ["(func (@name \"a\" \"b\"))", "1:18: @name annotation: unexpected token"],
    ["(module (@name \"a\") $m (@name \"b\"))", "1:24: @name annotation: multiple module names"],
    ["(func (@metadata.code.branch_hint \"\\02\") if end)", "1:7: @metadata.code.branch_hint annotation: invalid hint value"],
    ["(func (@metadata.code.branch_hint \"\\01\") nop)", "1:7: @metadata.code.branch_hint annotation: invalid target"],
    ["(func (@metadata.code.branch_hint \"\\01\") (nop))", "1:7: @metadata.code.branch_hint annotation: invalid target"],
    ["(global i32 (@metadata.code.branch_hint \"\\01\") (i32.const 0))", "1:13: @metadata.code.branch_hint annotation: not in a function"],
    ["(func (@metadata.code.branch_hint \"\\01\") (@metadata.code.branch_hint \"\\01\") if end)", "1:42: @metadata.code.branch_hint annotation: duplicate annotation"],
    ["(func (block (@custom \"x\")))", "1:14: misplaced @custom annotation"],
    ["(start $f (@name \"M\")) (func $f)", "1:11: misplaced @name annotation"],
    ["(data $d (@sym bss align=3))", "1:20: @sym annotation: alignment must be a power of two"],
    ["(data $d (@sym text))", "1:16: @sym annotation: unexpected token"],
    ["(data $d \"a\" (@sym))", "1:14: misplaced @sym annotation"],
    ["(data $d (@reloc 1))", "1:18: @reloc annotation: data id expected"],
    ["(data $d (@reloc $e x))", "1:21: @reloc annotation: unexpected token"],
    ["(global i32 (i32.const (@reloc $d)))", "1:24: misplaced @reloc annotation"],
    ["(func f32.const (@reloc $d) 0 drop)", "1:17: misplaced @reloc annotation"],
    ["(func $\"\\ff\")", "1:7: malformed UTF-8 encoding"],
];
{
    const inputs = Object.fromEntries(ERRORS.map(([src], k) => [`e${k}.wat`, src]));
    const r = as.run(Object.keys(inputs), inputs);
    const said = new Map();
    for (const l of r.err.split("\n").filter((l) => l))
        said.set(l.slice(0, l.indexOf(":")), l.slice(l.indexOf(":") + 1));
    ERRORS.forEach(([src, want], k) => {
        const got = said.get(`e${k}.wat`);
        const expect = want.replace(/^(\d+:\d+): /, "$1: error: ");
        if (got !== expect)
            bad.push(`${JSON.stringify(src)}: ${got ?? "accepted"}, expected ${expect}`);
    });
}

// ------------------------------------------------------------ nesting

// Parsed 10000 deep; printed 1000 deep, since the tree's indentation
// grows as the square of the depth.
for (const [n, args] of [[10000, []], [1000, ["--tree"]]]) {
    const deep = {
        "flat.wat": `(func ${"block ".repeat(n)}${"end ".repeat(n)})`,
        "folded.wat": `(func ${"(block ".repeat(n)}${")".repeat(n)})`,
        "operands.wat": `(func (drop ${"(i32.add (i32.const 1) ".repeat(n)}(i32.const 0)${")".repeat(n)}))`,
        "if.wat": `(func ${"(if (i32.const 0) (then ".repeat(n)}${"))".repeat(n)})`,
    };
    const r = as.run([...args, ...Object.keys(deep)], deep);
    const lines = r.out.split("\n").length;
    if (r.status !== 0 || r.err || (args.length && lines < 5 * n))
        bad.push(`nesting ${n} deep: status ${r.status}, ${lines} lines: ${r.err.slice(0, 200)}`);
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 60).join("\n  "));
console.log(`parser ok: ${TREES.length} golden trees, ${ERRORS.length} errors, nesting 10000 deep`);
