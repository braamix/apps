# wasm32 standard library headers

The C11 headers that are wasm32's own: `float.h`, `limits.h`, `setjmp.h`, `stdarg.h`,
`stddef.h`, `stdint.h`.  `inttypes.h` and `math.h` are shared with the other ILP32
targets in [libc/ilp32/include](../../ilp32/include), searched second, and the
target-neutral ones are in [libc/common/include](../../common/include), searched last:

```sh
cc -E -nostdinc -I libc/wasm32/include -I libc/ilp32/include -I libc/common/include prog.c prog.i
```

The data model is clang's `wasm32-unknown-unknown`: `int`, `long` and pointers are 32
bits, `long long` 64, plain `char` signed; `float` and `double` are IEEE binary32 and
binary64, `long double` binary128.  `<stdarg.h>` walks the buffer the caller fills with
the variable arguments; `setjmp`/`longjmp` are declared but not implemented.
