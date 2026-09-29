;; primes.s: the primes up to n, 100 by default, ten to a line.
;;
;;   as primes.s crt.s fmt.s args.s
;;   ar rc libw.a fmt.o args.o
;;   ld --allow-undefined crt.o primes.o -L. -lw -o primes
;;   primes 1000
;;
;; The sieve of Eratosthenes, a byte for each number, in a block from
;; crt.s's _alloc. n is at most ten million.
(module
  (import "env" "_alloc" (func $alloc (param i32) (result i32)))
  (import "env" "argc" (func $argc (param i32) (result i32)))
  (import "env" "arg" (func $arg (param i32 i32) (result i32 i32)))
  (import "env" "parse_u64" (func $parse_u64 (param i32 i32) (result i64)))
  (import "env" "out_str" (func $out_str (param i32 i32)))
  (import "env" "out_u64" (func $out_u64 (param i64)))
  (import "env" "out_byte" (func $out_byte (param i32)))
  (import "env" "out_nl" (func $out_nl))
  (memory 1)

  (data $usage (@sym rodata) "usage: primes [n], with n at most 10000000\n")

  (func $main (param $argv i32) (param $len i32) (result i32)
    (local $n64 i64) (local $n i32) (local $sieve i32) (local $i i32) (local $j i32)
    (local $count i32)
    (local.set $n64 (i64.const 100))
    (if (i32.gt_u (call $argc (local.get $argv)) (i32.const 1))
      (then
        (local.set $n64 (call $parse_u64 (call $arg (local.get $argv) (i32.const 1))))))
    (if (i32.or (i32.gt_u (call $argc (local.get $argv)) (i32.const 2))
                (i64.gt_u (local.get $n64) (i64.const 10000000)))
      (then
        (call $out_str (i32.const (@reloc $usage)) (i32.const 43))
        (return (i32.const 1))))
    (local.set $n (i32.wrap_i64 (local.get $n64)))

    ;; sieve[i] is 1 while i may be prime.
    (local.set $sieve (call $alloc (i32.add (local.get $n) (i32.const 1))))
    (if (i32.eqz (local.get $sieve))
      (then (unreachable)))
    (memory.fill (local.get $sieve) (i32.const 1) (i32.add (local.get $n) (i32.const 1)))
    (local.set $i (i32.const 2))
    (block $sifted
      (loop $prime
        (br_if $sifted (i32.gt_u (i32.mul (local.get $i) (local.get $i)) (local.get $n)))
        (if (i32.load8_u (i32.add (local.get $sieve) (local.get $i)))
          (then
            (local.set $j (i32.mul (local.get $i) (local.get $i)))
            (loop $multiple
              (i32.store8 (i32.add (local.get $sieve) (local.get $j)) (i32.const 0))
              (local.set $j (i32.add (local.get $j) (local.get $i)))
              (br_if $multiple (i32.le_u (local.get $j) (local.get $n))))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $prime)))

    (local.set $i (i32.const 2))
    (block $listed
      (loop $next
        (br_if $listed (i32.gt_u (local.get $i) (local.get $n)))
        (if (i32.load8_u (i32.add (local.get $sieve) (local.get $i)))
          (then
            (if (i32.rem_u (local.get $count) (i32.const 10))
              (then (call $out_byte (i32.const 32))))
            (call $out_u64 (i64.extend_i32_u (local.get $i)))
            (local.set $count (i32.add (local.get $count) (i32.const 1)))
            (if (i32.eqz (i32.rem_u (local.get $count) (i32.const 10)))
              (then (call $out_nl)))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (if (i32.rem_u (local.get $count) (i32.const 10))
      (then (call $out_nl)))
    (i32.const 0)))
