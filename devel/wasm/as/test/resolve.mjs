// as's resolution. First `as --resolved` of crafted modules, one for each
// rule of §6 and §5.4 and each tree rewrite wat.asdl lists, held to a
// golden file (BLESS=1 writes it). Then crafted errors, each with its
// message and place, and labels 10000 deep. Then the test suite: every
// module that is neither malformed nor invalid must be assembled, every
// assert_malformed module must be refused with its message, and an
// assert_invalid one may be refused only with its own.

import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assembler } from "../host.mjs";
import { modules } from "./wast.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const AS = join(HERE, "../../../../build/devel/wasm/as/as.wasm");
const SUITE = join(HERE, "suite/core");

function die(msg) {
    console.error("resolve: " + msg);
    process.exit(1);
}

if (!existsSync(AS))
    die(`no ${AS} — run make`);
const as = await assembler(AS);
const bad = [];

// ------------------------------------------------------------ golden trees

const TREES = [
    ["every space", `(type $t (func)) (func $f (type $t)) (table $tb 1 funcref) (memory $m 1)
(global $g (mut i32) (i32.const 0)) (tag $e) (elem $es func $f) (data $d "")
(func call $f ref.func $f table.size $tb memory.size $m global.set $g throw $e
  elem.drop $es data.drop $d table.init $tb $es memory.init $m $d call_ref $t
  i32.load $m table.copy $tb $tb memory.copy $m $m)
(export "f" (func $f)) (export "t" (table $tb)) (export "m" (memory $m))
(export "g" (global $g)) (export "e" (tag $e)) (start $f)`],
    ["forward", `(start $f) (export "g" (global $g)) (func call $f global.get $g) (func $f) (global $g i32 (i32.const 0))`],
    ["data annotations", `(memory 1) (data $a (@sym rodata) "abc") (data $b (@sym align=8) (@reloc $a 1))
(data $c (@sym) (i32.const 100) "c") (data $d (@sym align=4) "\\00") (data $e "e")
(func i32.const (@reloc $d -4) i32.load (@reloc $c) i64.const (@reloc $b) drop drop)`],
    ["data annotations, memory64", `(memory i64 1) (data $a (@sym) "abc") (func i64.const (@reloc $a 2) drop)`],
    ["imports first", `(import "m" "f" (func $i)) (import "m" "g" (global $h i32))
(func $f call $f call $i) (global $g i32 global.get $h) (export "f" (func $f)) (export "g" (global $g))`],
    ["locals", `(type $t (func (param i32 i64)))
(func (param $a i32) (param i64) (param $c f32) (local $d f64) (local v128) (local $f i32)
  local.get $a local.get 1 local.get $c local.get $d local.set $f local.tee 4)
(func (type $t) (local $x i32) local.get $x local.get 0)
(func (type $t) (param $p i32) (param $q i64) local.get $q)`],
    ["labels", `(func block $a loop $b br $a br $b br 0 br_if $a br_table $a $b 0 $a end end
  block $a block $a br $a end end
  (if $i (i32.const 0) (then br $i) (else br $i))
  block $x (block (br $x)) end)`],
    ["legacy try", `(tag $e) (func
  try $t catch $e rethrow $t catch_all try $u delegate $t rethrow $t end
  block $o try $v delegate $o end)`],
    ["try_table", `(tag $e) (func block $h (result exnref)
  try_table $t (catch $e $h) (catch_all_ref $h) br $t end unreachable end drop)`],
    ["fields", `(type $s (struct (field $a i32) (field i64) (field $c (mut f32))))
(type $p (struct (field $a f64)))
(func struct.get $s $a struct.get $s 1 struct.set $s $c struct.get $p $a struct.get 0 $c
  struct.get_s $s $c)`],
    ["implicit types in order of use", `(func (param i32) (result i32) block (result i32 i32) unreachable end
  call_indirect (param i64) block (param f32) drop end block (result f64) unreachable end drop unreachable)
(tag (param f32)) (func (param i32) (result i32) unreachable)
(func (type 2) block (param i32) (result i32) end)`],
    ["imports make types", `(import "m" "f" (func (param i64))) (import "m" "e" (tag (param i32)))
(import "m" "g" (func (param i64))) (func (param i32))`],
    ["explicit types matched", `(func (param i32)) (type (func (param i64))) (type $t (func (param i32)))
(func (param i64)) (type (sub (func (result i32)))) (func (result i32))
(rec (type (func (result f32)))) (rec (type (func (result f64))) (type (func)))
(func (result f32)) (func (result f64)) (func)`],
    ["type use filled", `(import "m" "f" (func (type $t))) (type $t (func (param i32) (result i64))) (type $u (func (result i32)))
(func (type $t) block (type $t) unreachable end block (type $u) unreachable end
  block (type 0) (param i32) (result i64) unreachable end drop
  call_indirect (type $t) call_indirect (type $t) (param i32) (result i64) drop)
(tag (type $u))`],
    ["reference types", `(rec (type $a (struct (field (ref $b)))) (type $b (sub (array (ref null $a)))))
(type $c (sub final $b (array (ref null $a))))
(global $g (ref null $a) (ref.null $a))
(func (param (ref $a)) (result (ref null $b)) (local (ref $c))
  ref.null $b ref.test (ref $a) ref.cast (ref null $b) br_on_cast 0 (ref null $a) (ref $a)
  select (result (ref null $a)) struct.new $a array.new_fixed $b 2 unreachable)
(table 1 (ref null $a))`],
    ["inline exports", `(export "a" (func 0)) (func $f (export "b") (export "c")) (export "d" (func $f))
(table (export "t") 0 funcref) (memory (export "m") 0) (global (export "g") i32 (i32.const 0))
(tag (export "e"))`],
    ["import exports", `(func (export "a") (import "m" "f")) (global $g (export "b") (export "c") (import "m" "g") i32)
(memory (export "m") (import "m" "m") 1) (func (export "d"))`],
    ["inline segments", `(elem $e0 func) (table $t funcref (elem $f)) (elem $e2 func)
(data $d0 "") (memory $m i64 (data "ab")) (data $d2 "")
(table i64 funcref (elem (item ref.func $f)))
(func $f elem.drop $e0 elem.drop $e2 data.drop $d0 data.drop $d2 table.init $t $e2)`],
    ["constant expressions", `(global $a i32 (i32.const 1)) (global $b i32 (global.get $a))
(table $t 1 funcref (ref.func $f)) (elem (table $t) (offset (global.get $a)) funcref (ref.func $f) (item global.get $c))
(global $c funcref (ref.func $f)) (data (memory $m) (offset global.get $a) "x") (memory $m 1) (func $f)`],
];

// [source, line:col: message]
const ERRORS = [
    ["(func call $f)", "1:12: unknown function $f"],
    ["(func (type $t))", "1:13: unknown type $t"],
    ["(func table.size $t)", "1:18: unknown table $t"],
    ["(func memory.size $m)", "1:19: unknown memory $m"],
    ["(func global.get $g)", "1:18: unknown global $g"],
    ["(func throw $e)", "1:13: unknown tag $e"],
    ["(func elem.drop $e)", "1:17: unknown elem segment $e"],
    ["(func data.drop $d)", "1:17: unknown data segment $d"],
    ["(func local.get $x)", "1:17: unknown local $x"],
    ["(func br $l)", "1:10: unknown label $l"],
    ["(type $s (struct (field $a i32))) (func struct.get $s $b)", "1:55: unknown field $b"],
    ["(type (func)) (func struct.get 0 $a)", "1:34: unknown field $a"],
    ["(func call $\"a b\")", "1:12: unknown function $\"a b\""],
    ["(export \"x\" (func $f))", "1:19: unknown function $f"],
    ["(start $f)", "1:8: unknown function $f"],
    ["(elem (table $t) (i32.const 0) func)", "1:14: unknown table $t"],
    ["(data (memory $m) (i32.const 0))", "1:15: unknown memory $m"],
    ["(global i32 local.get $x)", "1:23: unknown local $x"],
    ["(global i32 (block (br $l)))", "1:24: unknown label $l"],
    ["(func (param (ref $t)))", "1:19: unknown type $t"],
    ["(func try_table $l (catch_all $l) end)", "1:31: unknown label $l"],
    ["(func block $l try $t delegate $t end)", "1:32: unknown label $t"],
    ["(func block $a end br $a)", "1:23: unknown label $a"],
    ["(type $t (func)) (type $t (func))", "1:24: duplicate type $t"],
    ["(rec (type $t (func)) (type $t (func)))", "1:29: duplicate type $t"],
    ["(import \"a\" \"b\" (func $f)) (func $f)", "1:34: duplicate function $f"],
    ["(table $t 0 funcref) (table $t 0 funcref)", "1:29: duplicate table $t"],
    ["(memory $m 0) (memory $m 0)", "1:23: duplicate memory $m"],
    ["(global $g i32 (i32.const 0)) (global $g i32 (i32.const 0))", "1:39: duplicate global $g"],
    ["(tag $e) (tag $e)", "1:15: duplicate tag $e"],
    ["(elem $e func) (elem $e func)", "1:22: duplicate elem segment $e"],
    ["(data $d) (data $d)", "1:17: duplicate data segment $d"],
    ["(func (param $x i32) (param $x i32))", "1:29: duplicate local $x"],
    ["(func (param $x i32) (local $x i32))", "1:29: duplicate local $x"],
    ["(type (struct (field $a i32) (field $a i64)))", "1:37: duplicate field $a"],
    ["(type (func (param i32))) (func (type 0) (param i64))", "1:39: inline function type does not match explicit type"],
    ["(type (func)) (func (type 0) (result i32))", "1:27: inline function type does not match explicit type"],
    ["(type $t (func)) (func (block (type $t) (param i32)))", "1:37: inline function type does not match explicit type"],
    ["(type (func)) (func (call_indirect (type 0) (param i32)))", "1:42: inline function type does not match explicit type"],
    ["(func (type 1) (param i32))", "1:13: unknown type 1"],
    ["(type (struct)) (func (type 0) (param i32))", "1:29: non-function type 0"],
    ["(func (type 3) (local $x i32))", "1:13: unknown type 3"],
    ["(type (array i8)) (func (type 0) (local $x i32))", "1:31: non-function type 0"],
    ["(memory 1) (data (@sym) \"x\")", "1:13: @sym annotation: data segment without an id"],
    ["(data $d (@sym) \"x\")", "1:2: @sym annotation: no memory"],
    ["(memory 1) (data $d (@sym bss) \"x\")", "1:13: @sym annotation: bss data not zero"],
    ["(memory 1) (global i32 (i32.const 0)) (data $d (@sym) (global.get 0) \"x\")", "1:40: @sym annotation: offset not a constant"],
    ["(memory 1) (memory 1) (data $d (@sym) (memory 1) (i32.const 0) \"x\")", "1:24: @sym annotation: not in memory 0"],
    ["(memory 1) (data $d \"x\") (func i32.const (@reloc $d) drop)", "1:50: @reloc annotation: no @sym on data segment $d"],
    ["(memory 1) (func i32.load (@reloc $e) drop)", "1:35: unknown data segment $e"],
    ["(memory i64 1) (data $d (@sym) \"x\") (data $p (@sym) (@reloc $d))", "1:38: @reloc annotation: a 64-bit address in data"],
];

{
    const golden = join(HERE, "resolve.golden");
    const inputs = Object.fromEntries(TREES.map(([, src], k) => [`t${k}.s`, src]));
    const r = as.run(["--resolved", ...Object.keys(inputs)], inputs);
    if (r.status !== 0 || r.err)
        bad.push(`--resolved: status ${r.status}: ${r.err}`);
    const trees = r.out.split(/^(?=\(Module )/m);
    if (trees.length !== TREES.length)
        bad.push(`--resolved: ${trees.length} trees for ${TREES.length} modules`);
    const text = TREES.map(([name, src], k) => `=== ${name}\n${src}\n---\n${trees[k] ?? ""}`).join("\n");
    if (process.env.BLESS)
        writeFileSync(golden, text);
    else if (text !== readFileSync(golden, "utf8")) {
        const x = text.split("\n"), y = readFileSync(golden, "utf8").split("\n");
        const i = x.findIndex((l, k) => l !== y[k]);
        bad.push(`--resolved: line ${i + 1} is ${JSON.stringify(x[i])}, golden ${JSON.stringify(y[i])}`);
    }
}

{
    const inputs = Object.fromEntries(ERRORS.map(([src], k) => [`e${k}.s`, src]));
    const r = as.run(Object.keys(inputs), inputs);
    const said = new Map();
    for (const l of r.err.split("\n").filter((l) => l))
        said.set(l.slice(0, l.indexOf(":")), l.slice(l.indexOf(":") + 1));
    ERRORS.forEach(([src, want], k) => {
        const got = said.get(`e${k}.s`);
        const expect = want.replace(/^(\d+:\d+): /, "$1: error: ");
        if (got !== expect)
            bad.push(`${JSON.stringify(src)}: ${got ?? "accepted"}, expected ${expect}`);
    });
}

// ------------------------------------------------------------ nesting

// A branch out of n blocks, flat and folded: resolved 10000 deep, printed
// 1000 deep.
for (const [n, args] of [[10000, []], [1000, ["--resolved"]]]) {
    const deep = {
        "flat.s": `(func block $o ${"block ".repeat(n)}br $o ${"end ".repeat(n)}end)`,
        "folded.s": `(func (block $o ${"(block ".repeat(n)}(br $o)${")".repeat(n)}))`,
    };
    const r = as.run([...args, ...Object.keys(deep)], deep);
    const br = (r.out.match(new RegExp(`\\(Br br ${n} _\\)`, "g")) ?? []).length;
    if (r.status !== 0 || r.err || (args.length && br !== 2))
        bad.push(`nesting ${n} deep: status ${r.status}, ${br} branches: ${r.err.slice(0, 200)}`);
}

// ------------------------------------------------------------ the suite

const scripts = readdirSync(SUITE, { recursive: true }).filter((f) => f.endsWith(".wast")).sort();
const cases = [];
for (const f of scripts)
    for (const m of modules(readFileSync(join(SUITE, f))))
        if (m.kind !== "binary")
            cases.push({ ...m, script: f });

let refused = 0;
for (let at = 0; at < cases.length; at += 500) {
    const batch = cases.slice(at, at + 500);
    const inputs = Object.fromEntries(batch.map((c, k) => [`m${k}.s`, c.bytes]));
    const r = as.run(Object.keys(inputs), inputs);
    const said = new Map();
    for (const l of r.err.split("\n").filter((l) => l)) {
        const m = /^(m\d+\.s):\d+:\d+: error: (.*)$/.exec(l);
        if (!m)
            die(`as said: ${l}`);
        said.set(m[1], m[2]);
    }
    batch.forEach((c, k) => {
        const got = said.get(`m${k}.s`);
        const where = `${c.script}:${c.line}`;
        if (c.command === "assert_invalid") {
            // Resolved, then refused by validation, with its message.
            if (got && !got.startsWith(c.message))
                bad.push(`${where}: refused with "${got}", expected "${c.message}"`);
        } else if (c.command !== "assert_malformed") {
            if (got)
                bad.push(`${where}: ${c.command}: refused: ${got}`);
        } else if (!got) {
            bad.push(`${where}: accepted, expected "${c.message}"`);
        } else {
            refused++;
            if (!got.startsWith(c.message))
                bad.push(`${where}: refused with "${got}", expected "${c.message}"`);
        }
    });
}

if (bad.length)
    die(`${bad.length} failures:\n  ` + bad.slice(0, 60).join("\n  "));
console.log(`resolve ok: ${TREES.length} golden trees, ${ERRORS.length} errors, labels 10000 deep; ` +
            `${cases.length} modules of ${scripts.length} scripts, ${refused} malformed refused`);
