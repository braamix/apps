;; proc.s: what the kernel calls in a process besides _start and _resume.
;;
;;   as proc.s
;;   ar rc libw.a proc.o fmt.o args.o
;;
;; _alloc gives the kernel a block for the arguments or a reply, and 0
;; fails the step. The kernel never calls _free; a program does, when it
;; is done with a reply. _sig is a signal the program asked for.
;;
;; The heap starts where the linker's memory ends, and grows by pages.
;; A block is freed only if it is the last one allocated: as a stack.
(module
  (memory 1)

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

  ;; These programs ask for no signal.
  (func $_sig (export "_sig") (param $sig i32))

  ;; Up to a multiple of 8.
  (func $round (param $n i32) (result i32)
    (i32.and (i32.add (local.get $n) (i32.const 7)) (i32.const -8)))

  ;; Pages that hold n bytes.
  (func $pages (param $n i32) (result i32)
    (i32.shr_u (i32.add (local.get $n) (i32.const 0xffff)) (i32.const 16))))
