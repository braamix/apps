;; Data that C reads and writes: a counter, a greeting, a pointer to the
;; counter and a buffer; and a function that works on all of them.
(module
  (memory 1)
  (data $wat_counter (@sym align=4) "\05\00\00\00")
  (data $wat_greeting (@sym rodata) "hello from wat\00")
  (data $wat_counter_ptr (@sym align=4) (@reloc $wat_counter))
  (data $wat_buf (@sym bss align=4) "\00\00\00\00\00\00\00\00")
  ;; counter += 1; buf[1] = counter * 10; return *counter_ptr + buf[1]
  (func $wat_bump (result i32)
    (i32.store (i32.const (@reloc $wat_counter))
      (i32.add (i32.load (i32.const (@reloc $wat_counter))) (i32.const 1)))
    (i32.store (@reloc $wat_buf 4) (i32.const 0)
      (i32.mul (i32.load (i32.const (@reloc $wat_counter))) (i32.const 10)))
    (i32.add (i32.load (i32.load (i32.const (@reloc $wat_counter_ptr))))
             (i32.load (@reloc $wat_buf 4) (i32.const 0)))))
