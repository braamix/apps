;; A function that calls C, and keeps a count in a global of its own.
(module
  (import "env" "c_twice" (func $c_twice (param i32) (result i32)))
  (import "env" "fx_putn" (func $fx_putn (param i32)))
  (global $calls (mut i32) (i32.const 0))
  (func $wat_report
    (global.set $calls (i32.add (global.get $calls) (i32.const 1)))
    (call $fx_putn (call $c_twice (i32.const 21)))
    (call $fx_putn (global.get $calls))))
