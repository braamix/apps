;; fib.s: the Fibonacci numbers up to fib(n), 30 by default.
;;
;;   as fib.s crt.s fmt.s args.s
;;   ar rc libw.a fmt.o args.o
;;   ld crt.o fib.o -L. -lw -o fib
;;   fib 10
;;
;; A table, added up in i64, then the last of it again by recursion,
;; for up to fib(25). fib(93) is the last that fits in 64 bits.
(module
  (import "env" "argc" (func $argc (param i32) (result i32)))
  (import "env" "arg" (func $arg (param i32 i32) (result i32 i32)))
  (import "env" "parse_u64" (func $parse_u64 (param i32 i32) (result i64)))
  (import "env" "out_str" (func $out_str (param i32 i32)))
  (import "env" "out_u64" (func $out_u64 (param i64)))
  (import "env" "out_byte" (func $out_byte (param i32)))
  (import "env" "out_nl" (func $out_nl))
  (memory 1)

  (data $usage (@sym rodata) "usage: fib [n], with n at most 93\n")
  (data $fib (@sym rodata) "fib(")
  (data $is (@sym rodata) ") = ")
  (data $by (@sym rodata) " by recursion")

  (func $slow (param $n i32) (result i64)
    (if (result i64) (i32.lt_u (local.get $n) (i32.const 2))
      (then (i64.extend_i32_u (local.get $n)))
      (else
        (i64.add (call $slow (i32.sub (local.get $n) (i32.const 1)))
                 (call $slow (i32.sub (local.get $n) (i32.const 2)))))))

  ;; fib(n) = v
  (func $line (param $n i32) (param $v i64)
    (call $out_str (i32.const (@reloc $fib)) (i32.const 4))
    (call $out_u64 (i64.extend_i32_u (local.get $n)))
    (call $out_str (i32.const (@reloc $is)) (i32.const 4))
    (call $out_u64 (local.get $v)))

  (func $main (param $argv i32) (param $len i32) (result i32)
    (local $n i64) (local $i i32) (local $a i64) (local $b i64) (local $t i64)
    (local.set $n (i64.const 30))
    (if (i32.gt_u (call $argc (local.get $argv)) (i32.const 1))
      (then
        (local.set $n (call $parse_u64 (call $arg (local.get $argv) (i32.const 1))))))
    (if (i32.or (i32.gt_u (call $argc (local.get $argv)) (i32.const 2))
                (i64.gt_u (local.get $n) (i64.const 93)))
      (then
        (call $out_str (i32.const (@reloc $usage)) (i32.const 34))
        (return (i32.const 1))))
    (local.set $b (i64.const 1))
    (loop $next
      (call $line (local.get $i) (local.get $a))
      (call $out_nl)
      (local.set $t (i64.add (local.get $a) (local.get $b)))
      (local.set $a (local.get $b))
      (local.set $b (local.get $t))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $next (i64.le_u (i64.extend_i32_u (local.get $i)) (local.get $n))))
    (local.set $i (select (i32.wrap_i64 (local.get $n)) (i32.const 25)
                          (i64.le_u (local.get $n) (i64.const 25))))
    (call $line (local.get $i) (call $slow (local.get $i)))
    (call $out_str (i32.const (@reloc $by)) (i32.const 13))
    (call $out_nl)
    (i32.const 0)))
