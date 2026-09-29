;; cat.s: copies its standard input to its standard output.
;;
;;   as cat.s
;;   ld --allow-undefined cat.o -o cat
;;
;; A program cannot wait here: it asks, returns from the step, and is
;; resumed with the answer. So cat is a loop turned inside out. Each
;; call is made with a token, and _resume goes by the token to the
;; code that continues from that call. hello.s says what the rest is.
(module
  (import "kernel" "sys" (func $sys (param i32 i32 i32 i32) (result i32)))
  (import "kernel" "sys_async" (func $sys_async (param i32 i32 i32 i32)))
  (memory 1)

  ;; A read's reply: the block, its length, and the bytes not yet written.
  (global $block (mut i32) (i32.const 0))
  (global $block_len (mut i32) (i32.const 0))
  (global $at (mut i32) (i32.const 0))
  (global $left (mut i32) (i32.const 0))

  ;; The tokens: which call a reply answers.
  (global $READ i32 (i32.const 1))
  (global $WRITE i32 (i32.const 2))

  ;; Read (17) from fd 0, stdin, up to 512 bytes. The reply is an i32,
  ;; the count read, then the bytes; a count of 0 is the end.
  (func $read
    (call $sys_async (i32.const 17) (global.get $READ) (i32.const 0) (i32.const 0)))

  (func $write
    (call $sys_async (i32.const 272) (global.get $WRITE) (global.get $at) (global.get $left)))

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

  (func $_start (export "_start") (param $argv i32) (param $len i32) (result i32)
    (call $read)
    (i32.const 1))

  (func $_resume (export "_resume") (param $token i32) (param $reply i32) (param $len i32)
                                    (result i32)
    (local $n i32)
    (local.set $n (i32.load (local.get $reply)))
    (block $wrote
      (block $read
        (block $bad
          (br_table $bad $read $wrote $bad (local.get $token)))
        (unreachable))

      ;; Some bytes read, or the end. The block is kept until they are
      ;; written; the write's reply is allocated above it.
      (if (i32.le_s (local.get $n) (i32.const 0))
        (then (return (call $exit (i32.lt_s (local.get $n) (i32.const 0))))))
      (global.set $block (local.get $reply))
      (global.set $block_len (local.get $len))
      (global.set $at (i32.add (local.get $reply) (i32.const 4)))
      (global.set $left (local.get $n))
      (call $write)
      (return (i32.const 1)))

    ;; Some bytes written: the rest, or the next read.
    (call $_free (local.get $reply) (local.get $len))
    (if (i32.le_s (local.get $n) (i32.const 0))
      (then (return (call $exit (i32.const 1)))))
    (global.set $at (i32.add (global.get $at) (local.get $n)))
    (global.set $left (i32.sub (global.get $left) (local.get $n)))
    (if (i32.gt_u (global.get $left) (i32.const 0))
      (then (call $write))
      (else
        (call $_free (global.get $block) (global.get $block_len))
        (call $read)))
    (i32.const 1)))
