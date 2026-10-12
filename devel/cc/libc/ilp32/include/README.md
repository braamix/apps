# ILP32 standard library headers

The C11 headers that depend only on the ILP32 data model shared by riscv32, arm32 and
wasm32: `inttypes.h`, `limits.h`, `math.h`.  They are searched after the target's own
directory ([libc/riscv32/include](../../riscv32/include),
[libc/arm32/include](../../arm32/include) or
[libc/wasm32/include](../../wasm32/include), which has its own `limits.h` for its signed
`char`) and before
[libc/common/include](../../common/include):

```sh
cc -E -nostdinc -I libc/riscv32/include -I libc/ilp32/include -I libc/common/include prog.c prog.i
```

`int`, `long` and pointers are 32 bits, `long long` 64, plain `char` unsigned.
`float.h` is not here: `long double` is binary128 on RISC-V and wasm32 but binary64 on ARM32;
nor are `stddef.h` and `stdint.h`: `wchar_t` is `int` on RISC-V but `unsigned int` on
ARM32.  The C sources of the ILP32 libc routines are in the parent directory.
