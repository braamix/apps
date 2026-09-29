;; hello2.s: hello.s again, on crt.o.
;;
;;   as hello2.s
;;   ld crt.o hello2.o -lw -o hello2
;;
;; main prints into fmt.s's buffer and returns the exit status. crt.o
;; writes the buffer and exits, so there is no _start or _resume here.
(module
  (import "env" "out_str" (func $out_str (param i32 i32)))
  (memory 1)

  (data $msg (@sym rodata) "hello, world\n")

  (func $main (param $argv i32) (param $len i32) (result i32)
    (call $out_str (i32.const (@reloc $msg)) (i32.const 13))
    (i32.const 0)))
