;; hello2.s: hello.s again, with libw.a doing the writing.
;;
;;   as hello2.s
;;   ld hello2.o -lw -o hello2
;;
;; _start prints into fmt.s's buffer, and asks for it to be written. Each
;; answer goes to out_wrote, which asks for the rest after a short write.
;; libw.a's proc.s has _alloc, _free and _sig.
(module
  (import "kernel" "sys" (func $sys (param i32 i32 i32 i32) (result i32)))
  (import "env" "_free" (func $_free (param i32 i32)))
  (import "env" "out_str" (func $out_str (param i32 i32)))
  (import "env" "out_flush" (func $out_flush (param i32) (result i32)))
  (import "env" "out_wrote" (func $out_wrote (param i32) (result i32)))
  (memory 1)

  (data $msg (@sym rodata) "hello, world\n")

  (func $exit (param $status i32) (result i32)
    (drop (call $sys (i32.const 1) (local.get $status) (i32.const 0) (i32.const 0)))
    (i32.const 0))

  (func $_start (export "_start") (param $argv i32) (param $len i32) (result i32)
    (call $out_str (i32.const (@reloc $msg)) (i32.const 13))
    (if (call $out_flush (i32.const 1))
      (then (return (i32.const 1))))
    (call $exit (i32.const 0)))

  (func $_resume (export "_resume") (param $token i32) (param $reply i32) (param $len i32)
                                    (result i32)
    (local $more i32)
    (local.set $more (call $out_wrote (local.get $reply)))
    (call $_free (local.get $reply) (local.get $len))
    (if (i32.gt_s (local.get $more) (i32.const 0))
      (then (return (i32.const 1))))
    (call $exit (i32.lt_s (local.get $more) (i32.const 0)))))
