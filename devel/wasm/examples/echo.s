;; echo.s: writes its arguments, separated by spaces, and a newline.
;;
;;   as echo.s
;;   ld --allow-undefined echo.o -o echo
;;
;; The arguments come to _start as one block: a u32 count, then each
;; argument as a u32 length and its bytes. The environment follows in
;; the same form. hello.s says what the rest of this is.
(module
  (import "kernel" "sys" (func $sys (param i32 i32 i32 i32) (result i32)))
  (import "kernel" "sys_async" (func $sys_async (param i32 i32 i32 i32)))
  (memory 1)

  ;; The line to write, and how much of it is written.
  (global $line (mut i32) (i32.const 0))
  (global $line_len (mut i32) (i32.const 0))
  (global $done (mut i32) (i32.const 0))

  (func $write
    (call $sys_async (i32.const 272) (i32.const 1)
      (i32.add (global.get $line) (global.get $done))
      (i32.sub (global.get $line_len) (global.get $done))))

  (func $exit (param $status i32) (result i32)
    (drop (call $sys (i32.const 1) (local.get $status) (i32.const 0) (i32.const 0)))
    (i32.const 0))

  (global $brk (mut i32) (i32.const 0))

  (func $_alloc (export "_alloc") (param $n i32) (result i32)
    (local $p i32) (local $end i32) (local $top i32)
    (local.set $top (i32.shl (memory.size) (i32.const 16)))
    (if (i32.eqz (global.get $brk))
      (then (global.set $brk (local.get $top))))
    (local.set $p (global.get $brk))
    (local.set $end (call $round (i32.add (local.get $p) (local.get $n))))
    (if (i32.gt_u (local.get $end) (local.get $top))
      (then
        (if (i32.eq (memory.grow (call $pages (i32.sub (local.get $end) (local.get $top))))
                    (i32.const -1))
          (then (return (i32.const 0))))))
    (global.set $brk (local.get $end))
    (local.get $p))

  (func $_free (export "_free") (param $p i32) (param $n i32)
    (if (i32.eq (global.get $brk) (call $round (i32.add (local.get $p) (local.get $n))))
      (then (global.set $brk (local.get $p)))))

  (func $round (param $n i32) (result i32)
    (i32.and (i32.add (local.get $n) (i32.const 7)) (i32.const -8)))

  (func $pages (param $n i32) (result i32)
    (i32.shr_u (i32.add (local.get $n) (i32.const 0xffff)) (i32.const 16)))

  (func $_sig (export "_sig") (param $sig i32))

  ;; The line is never longer than the block: each argument there has
  ;; four bytes of length, and here one byte after it.
  (func $_start (export "_start") (param $argv i32) (param $len i32) (result i32)
    (local $argc i32) (local $i i32) (local $at i32) (local $n i32) (local $out i32)
    (global.set $line (call $_alloc (local.get $len)))
    (if (i32.eqz (global.get $line))
      (then (return (call $exit (i32.const 1)))))
    (local.set $out (global.get $line))
    (local.set $argc (i32.load (local.get $argv)))
    (local.set $at (i32.add (local.get $argv) (i32.const 4)))
    (local.set $i (i32.const 0))
    (block $end
      (loop $next
        (br_if $end (i32.ge_u (local.get $i) (local.get $argc)))
        (local.set $n (i32.load (local.get $at)))
        (local.set $at (i32.add (local.get $at) (i32.const 4)))
        ;; argv[0] is the command's name.
        (if (i32.gt_u (local.get $i) (i32.const 0))
          (then
            (if (i32.gt_u (local.get $i) (i32.const 1))
              (then
                (i32.store8 (local.get $out) (i32.const 32))
                (local.set $out (i32.add (local.get $out) (i32.const 1)))))
            (memory.copy (local.get $out) (local.get $at) (local.get $n))
            (local.set $out (i32.add (local.get $out) (local.get $n)))))
        (local.set $at (i32.add (local.get $at) (local.get $n)))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.store8 (local.get $out) (i32.const 10))
    (global.set $line_len
      (i32.sub (i32.add (local.get $out) (i32.const 1)) (global.get $line)))
    (call $write)
    (i32.const 1))

  (func $_resume (export "_resume") (param $token i32) (param $reply i32) (param $len i32)
                                    (result i32)
    (local $n i32)
    (local.set $n (i32.load (local.get $reply)))
    (call $_free (local.get $reply) (local.get $len))
    (if (i32.le_s (local.get $n) (i32.const 0))
      (then (return (call $exit (i32.const 1)))))
    (global.set $done (i32.add (global.get $done) (local.get $n)))
    (if (i32.lt_u (global.get $done) (global.get $line_len))
      (then
        (call $write)
        (return (i32.const 1))))
    (call $exit (i32.const 0))))
