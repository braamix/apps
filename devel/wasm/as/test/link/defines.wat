;; Functions that C calls.
(module
  (func $wat_fib (param $n i32) (result i32)
    (if (result i32) (i32.lt_s (local.get $n) (i32.const 2))
      (then (local.get $n))
      (else
        (i32.add (call $wat_fib (i32.sub (local.get $n) (i32.const 1)))
                 (call $wat_fib (i32.sub (local.get $n) (i32.const 2)))))))
  (func $wat_sum (param i32 i32) (result i32)
    (i32.add (local.get 0) (local.get 1))))
