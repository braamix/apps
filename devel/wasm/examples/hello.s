;; hello.s: the smallest Braam program, written by hand.
;;
;;   as hello.s
;;   ld hello.o -o hello
;;
;; A process is a module the kernel steps. It calls _start once, then
;; _resume once for each reply to a call made with sys_async. Each step
;; returns 1 while a call is outstanding, and 0 when the process is done.
;; A reply is placed in a block the kernel asks _alloc for.
(module
  ;; sys(op, a0, a1, a2) is answered at once; sys_async(op, token, ptr,
  ;; len) is answered by a later _resume(token, reply, len).
  (import "kernel" "sys" (func $sys (param i32 i32 i32 i32) (result i32)))
  (import "kernel" "sys_async" (func $sys_async (param i32 i32 i32 i32)))

  ;; The linker makes this the process's memory, env.memory.
  (memory 1)

  (data $msg (@sym rodata) "hello, world\n")
  (global $msg_len i32 (i32.const 13))

  ;; Bytes of the message written so far.
  (global $done (mut i32) (i32.const 0))

  ;; Write the rest of the message. The op is Write (16) with the fd
  ;; above the low byte: 16 | 1 << 8 is stdout. The kernel copies the
  ;; bytes at once; its reply is an i32, the count written.
  (func $write
    (call $sys_async (i32.const 272) (i32.const 1)
      (i32.add (i32.const (@reloc $msg)) (global.get $done))
      (i32.sub (global.get $msg_len) (global.get $done))))

  ;; Exit (1) records the status; the step then returns 0.
  (func $exit (param $status i32) (result i32)
    (drop (call $sys (i32.const 1) (local.get $status) (i32.const 0) (i32.const 0)))
    (i32.const 0))

  ;; The heap starts where the linker's memory ends and grows by pages.
  ;; A block is freed only if it is the last one allocated.
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

  ;; Up to a multiple of 8.
  (func $round (param $n i32) (result i32)
    (i32.and (i32.add (local.get $n) (i32.const 7)) (i32.const -8)))

  ;; Pages that hold n bytes.
  (func $pages (param $n i32) (result i32)
    (i32.shr_u (i32.add (local.get $n) (i32.const 0xffff)) (i32.const 16)))

  ;; A signal this process asked for; it asks for none.
  (func $_sig (export "_sig") (param $sig i32))

  (func $_start (export "_start") (param $argv i32) (param $len i32) (result i32)
    (call $write)
    (i32.const 1))

  ;; The one call there is to answer is a write. A write may be short.
  (func $_resume (export "_resume") (param $token i32) (param $reply i32) (param $len i32)
                                    (result i32)
    (local $n i32)
    (local.set $n (i32.load (local.get $reply)))
    (call $_free (local.get $reply) (local.get $len))
    (if (i32.le_s (local.get $n) (i32.const 0))
      (then (return (call $exit (i32.const 1)))))
    (global.set $done (i32.add (global.get $done) (local.get $n)))
    (if (i32.lt_u (global.get $done) (global.get $msg_len))
      (then
        (call $write)
        (return (i32.const 1))))
    (call $exit (i32.const 0))))
