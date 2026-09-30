;; fmt.s: output into a buffer, and writing it; and numbers.
;;
;;   as fmt.s
;;   ar rc libw.a proc.o fmt.o args.o
;;
;; The buffer is allocated with proc.s's _alloc, and doubles when full.
;; If memory runs out, the program traps.
;;
;; A program prints with out_*, then writes it all at the end: its
;; _start calls out_flush, and its _resume gives each answer to
;; out_wrote. hello2.s shows how.
(module
  (import "kernel" "sys_async" (func $sys_async (param i32 i32 i32 i32)))
  (import "env" "_alloc" (func $alloc (param i32) (result i32)))
  (import "env" "_free" (func $free (param i32 i32)))
  (memory 1)

  (global $buf (mut i32) (i32.const 0))
  (global $len (mut i32) (i32.const 0))
  (global $cap (mut i32) (i32.const 0))

  ;; What is not yet written, and where to.
  (global $fd (mut i32) (i32.const 1))
  (global $at (mut i32) (i32.const 0))
  (global $left (mut i32) (i32.const 0))

  ;; Write (16) the rest to the fd: 16 | fd << 8. Token 1.
  (func $write
    (call $sys_async (i32.or (i32.const 16) (i32.shl (global.get $fd) (i32.const 8)))
                     (i32.const 1) (global.get $at) (global.get $left)))

  ;; Starts writing what was printed to `fd`, 1 for stdout or 2 for
  ;; stderr. 1: a write is asked for, and out_wrote takes its answer.
  ;; 0: there is nothing to write. Called once, at the end.
  (func $out_flush (param $fd i32) (result i32)
    (global.set $fd (local.get $fd))
    (global.set $at (global.get $buf))
    (global.set $left (global.get $len))
    (if (i32.eqz (global.get $left))
      (then (return (i32.const 0))))
    (call $write)
    (i32.const 1))

  ;; A write's answer, the count written. 1: a write may be short, and
  ;; the rest is asked for. 0: all is written. -1: the write failed.
  (func $out_wrote (param $reply i32) (result i32)
    (local $n i32)
    (local.set $n (i32.load (local.get $reply)))
    (if (i32.le_s (local.get $n) (i32.const 0))
      (then (return (i32.const -1))))
    (global.set $at (i32.add (global.get $at) (local.get $n)))
    (global.set $left (i32.sub (global.get $left) (local.get $n)))
    (if (i32.eqz (global.get $left))
      (then (return (i32.const 0))))
    (call $write)
    (i32.const 1))

  ;; Room for n more bytes. The old buffer is given back first: if it
  ;; was the last block, the new one starts where it did, and nothing
  ;; moves.
  (func $reserve (param $n i32)
    (local $need i32) (local $cap i32) (local $p i32)
    (local.set $need (i32.add (global.get $len) (local.get $n)))
    (if (i32.le_u (local.get $need) (global.get $cap))
      (then (return)))
    (local.set $cap (select (global.get $cap) (i32.const 256) (global.get $cap)))
    (block $big
      (loop $double
        (br_if $big (i32.ge_u (local.get $cap) (local.get $need)))
        (local.set $cap (i32.shl (local.get $cap) (i32.const 1)))
        (br $double)))
    (call $free (global.get $buf) (global.get $cap))
    (local.set $p (call $alloc (local.get $cap)))
    (if (i32.eqz (local.get $p))
      (then (unreachable)))
    (memory.copy (local.get $p) (global.get $buf) (global.get $len))
    (global.set $buf (local.get $p))
    (global.set $cap (local.get $cap)))

  (func $out_byte (param $b i32)
    (call $reserve (i32.const 1))
    (i32.store8 (i32.add (global.get $buf) (global.get $len)) (local.get $b))
    (global.set $len (i32.add (global.get $len) (i32.const 1))))

  (func $out_str (param $p i32) (param $n i32)
    (call $reserve (local.get $n))
    (memory.copy (i32.add (global.get $buf) (global.get $len)) (local.get $p) (local.get $n))
    (global.set $len (i32.add (global.get $len) (local.get $n))))

  (func $out_nl
    (call $out_byte (i32.const 10)))

  ;; In decimal: the digits are counted, then written from the last.
  (func $out_u64 (param $v i64)
    (local $digits i32) (local $t i64) (local $at i32)
    (local.set $digits (i32.const 1))
    (local.set $t (local.get $v))
    (block $counted
      (loop $count
        (br_if $counted (i64.lt_u (local.get $t) (i64.const 10)))
        (local.set $t (i64.div_u (local.get $t) (i64.const 10)))
        (local.set $digits (i32.add (local.get $digits) (i32.const 1)))
        (br $count)))
    (call $reserve (local.get $digits))
    (global.set $len (i32.add (global.get $len) (local.get $digits)))
    (local.set $at (i32.add (global.get $buf) (global.get $len)))
    (loop $digit
      (local.set $at (i32.sub (local.get $at) (i32.const 1)))
      (i64.store8 (local.get $at)
        (i64.add (i64.rem_u (local.get $v) (i64.const 10)) (i64.const 48)))
      (local.set $v (i64.div_u (local.get $v) (i64.const 10)))
      (br_if $digit (i64.ne (local.get $v) (i64.const 0)))))

  ;; A decimal number of up to 19 digits, or -1 if the bytes are not one.
  (func $parse_u64 (param $p i32) (param $n i32) (result i64)
    (local $v i64) (local $c i32) (local $end i32)
    (if (i32.or (i32.eqz (local.get $n)) (i32.gt_u (local.get $n) (i32.const 19)))
      (then (return (i64.const -1))))
    (local.set $end (i32.add (local.get $p) (local.get $n)))
    (loop $digit
      (local.set $c (i32.sub (i32.load8_u (local.get $p)) (i32.const 48)))
      (if (i32.gt_u (local.get $c) (i32.const 9))
        (then (return (i64.const -1))))
      (local.set $v (i64.add (i64.mul (local.get $v) (i64.const 10))
                             (i64.extend_i32_u (local.get $c))))
      (local.set $p (i32.add (local.get $p) (i32.const 1)))
      (br_if $digit (i32.lt_u (local.get $p) (local.get $end))))
    (local.get $v)))
