;; hello.s: the smallest Braam program, written by hand.
;;
;;   as hello.s
;;   ld hello.o -lw -o hello
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

  ;; The kernel asks _alloc for a block for each reply, and _free gives
  ;; one back. They are in libw.a, with _sig: proc.s. This program calls
  ;; only _free, and that links them all.
  (import "env" "_free" (func $_free (param i32 i32)))

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
