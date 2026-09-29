;; crt.s: the start of a program that computes, prints and exits.
;;
;;   as crt.s
;;   ld crt.o prog.o -lw -o prog
;;
;; _start calls main(argv, len), which returns the exit status. What
;; main printed with fmt.s is then written in one go: to stdout if the
;; status is 0, and to stderr if not. So main never waits, and is a
;; plain function.
;;
;; crt.o is named on the command line, not put in libw.a: a library's
;; member is linked only for a symbol something else needs, and nothing
;; needs _start. hello.s says what the rest of this is.
(module
  (import "kernel" "sys" (func $sys (param i32 i32 i32 i32) (result i32)))
  (import "kernel" "sys_async" (func $sys_async (param i32 i32 i32 i32)))
  (import "env" "_free" (func $_free (param i32 i32)))
  (import "env" "main" (func $main (param i32 i32) (result i32)))
  (import "env" "out_buf" (func $out_buf (result i32 i32)))
  (memory 1)

  (global $status (mut i32) (i32.const 0))
  (global $at (mut i32) (i32.const 0))
  (global $left (mut i32) (i32.const 0))

  ;; Write (16) to fd 1, or to fd 2 if the status is not 0.
  (func $write
    (call $sys_async
      (select (i32.const 528) (i32.const 272) (global.get $status))
      (i32.const 1) (global.get $at) (global.get $left)))

  (func $exit (param $status i32) (result i32)
    (drop (call $sys (i32.const 1) (local.get $status) (i32.const 0) (i32.const 0)))
    (i32.const 0))

  (func $_start (export "_start") (param $argv i32) (param $len i32) (result i32)
    (global.set $status (call $main (local.get $argv) (local.get $len)))
    (call $out_buf)
    (global.set $left)
    (global.set $at)
    (if (i32.eqz (global.get $left))
      (then (return (call $exit (global.get $status)))))
    (call $write)
    (i32.const 1))

  (func $_resume (export "_resume") (param $token i32) (param $reply i32) (param $len i32)
                                    (result i32)
    (local $n i32)
    (local.set $n (i32.load (local.get $reply)))
    (call $_free (local.get $reply) (local.get $len))
    (if (i32.le_s (local.get $n) (i32.const 0))
      (then (return (call $exit (i32.const 1)))))
    (global.set $at (i32.add (global.get $at) (local.get $n)))
    (global.set $left (i32.sub (global.get $left) (local.get $n)))
    (if (i32.gt_u (global.get $left) (i32.const 0))
      (then
        (call $write)
        (return (i32.const 1))))
    (call $exit (global.get $status))))
