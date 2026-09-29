;; args.s: the arguments crt.s passes to main.
;;
;;   as args.s
;;   ar rc libw.a fmt.o args.o
;;
;; They are one block: a u32 count, then each argument as a u32 length
;; and its bytes. argv[0] is the command's name.
(module
  (memory 1)

  (func $argc (param $argv i32) (result i32)
    (i32.load (local.get $argv)))

  ;; Argument i: where its bytes are, and how many. i must be below argc.
  (func $arg (param $argv i32) (param $i i32) (result i32 i32)
    (local $at i32)
    (local.set $at (i32.add (local.get $argv) (i32.const 4)))
    (block $found
      (loop $skip
        (br_if $found (i32.eqz (local.get $i)))
        (local.set $at (i32.add (local.get $at)
                                (i32.add (i32.const 4) (i32.load (local.get $at)))))
        (local.set $i (i32.sub (local.get $i) (i32.const 1)))
        (br $skip)))
    (i32.add (local.get $at) (i32.const 4))
    (i32.load (local.get $at))))
