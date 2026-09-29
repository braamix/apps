# Programming Braam in assembly

This tutorial explains two small programs, line by line. Both print
`hello, world`. [examples/hello2.s](examples/hello2.s) is the easy one:
a library does the hard part. [examples/hello.s](examples/hello.s) does
everything itself, and shows how a Braam program really works.

You need the `wasm` package. Install it, and copy the examples out:

    $ pkg install wasm
    $ cp /pkg/store/wasm-*/share/hello*.s .

Then assemble each program with `as`, and link it with `ld`:

    $ as hello2.s
    $ ld crt.o hello2.o -lw -o hello2
    $ ./hello2
    hello, world
    $ as hello.s
    $ ld hello.o -lw -o hello
    $ ./hello
    hello, world

`as hello2.s` writes `hello2.o`. `ld` joins it with `crt.o` and the
library `libw.a`, which `-lw` names, into the program `hello2`. `ld`
finds `crt.o` and `libw.a` in the package. 

## 1. What you are writing

A Braam program is a **WebAssembly module**, and the kernel runs it. You
write it in WebAssembly's *text format*, in a file ending in `.s`. Two
tools turn it into a program:

- `as` assembles `hello.s` into an **object**, `hello.o`;
- `ld` links objects and libraries into a **program**, `hello`.

WebAssembly is a **stack machine**. An instruction takes its operands
from a stack and pushes its result back. So `1 + 2` is:

    i32.const 1    ;; push 1
    i32.const 2    ;; push 2
    i32.add        ;; pop both, push 3

The text format lets you write the same thing *folded*, operands inside
parentheses after the instruction:

    (i32.add (i32.const 1) (i32.const 2))

The examples use the folded form. Read it inside out: the inner
parentheses run first.

A few more words you will see:

- **`i32`, `i64`**: 32-bit and 64-bit integers. Addresses are `i32`.
- **`$name`**: a name you give to a function, variable or data. The
  `$` is part of it.
- **`;;`**: a comment, to the end of the line.
- **`_s` and `_u`**: signed and unsigned. `i32.lt_u` compares as
  unsigned, `i32.le_s` as signed.

## 2. hello2.s: the easy way

    ;; hello2.s: hello.s again, on crt.o.

Lines that start with `;;` are comments. The first few say what the file
is and how to build it:

    ;;   as hello2.s
    ;;   ld crt.o hello2.o -lw -o hello2

`crt.o` and `-lw` are the helpers this program leans on. `ld` finds both
in the package's `lib/`. [Section 4](#4-what-crto-does) explains them.

    (module

Everything is inside one `module`. The last `)` in the file closes it.

    (import "env" "out_str" (func $out_str (param i32 i32)))

This line borrows a function from another object. `out_str` is in
`libw.a`: it takes the address of some bytes, and how many there are,
and adds them to the output. `(param i32 i32)` says it takes two `i32`s
and returns nothing. We call it `$out_str` here.

The module name `"env"` means "another object linked with this one".
`ld` finds `out_str` in `libw.a` because of `-lw`.

    (memory 1)

The program's memory: one **page**, 64 KiB, at least. The memory is an
array of bytes, and an address is an index into it. `ld` makes this the
memory the kernel gives the process.

    (data $msg (@sym rodata) "hello, world\n")

Thirteen bytes of text, put in memory. `\n` is one byte, a newline.
`(@sym rodata)` lets `ld` choose where they go: it makes the bytes a
*symbol* named `msg`, which is read-only data. We do not know the address
yet; `ld` will.

    (func $main (param $argv i32) (param $len i32) (result i32)

A function named `$main`. It takes two `i32` parameters and returns one
`i32`. `crt.o` calls it, and passes the command's arguments in `$argv`
and `$len`. This program ignores them.

`$main` is not exported. It does not have to be: it is a symbol, and
`ld` connects it to the `main` that `crt.o` imports.

    (call $out_str (i32.const (@reloc $msg)) (i32.const 13))

Call `out_str` with two numbers. The first is the address of `$msg`.
`(@reloc $msg)` stands in place of a number here: `ld` writes in the
address when it places the text. The second is `13`, the length of
`hello, world\n`.

    (i32.const 0)))

The last value on the stack is what the function returns: `0`, which
means success. `crt.o` makes it the exit status. The three `)` close the
`func` and the `module`.

That is the whole program. It never waits and never talks to the kernel.
`crt.o` does that part.

## 3. hello.s: the whole story

`hello.s` talks to the kernel itself. First, how the kernel runs a
program.

### How the kernel runs a program

A program cannot stop and wait. Suppose it asks the kernel to write some
text. It does not wait for the answer. It *returns* instead, and the
kernel calls it again later with the answer. Each call from the kernel
is a **step**:

- the first step is the function `_start`;
- each later step is `_resume`, with one answer.

A step returns `1` if the program is waiting for an answer, and `0` when
the program is finished.

The program asks the kernel for things through two functions the kernel
provides:

- `sys` is answered at once, as its return value;
- `sys_async` is answered later, by a call of `_resume`.

For `hello`, the steps are:

| Step | The program |
| --- | --- |
| `_start` | asks the kernel to write the text; returns 1 |
| `_resume` | gets how many bytes were written; says "exit with 0"; returns 0 |

Now the code.

### The imports

    (import "kernel" "sys" (func $sys (param i32 i32 i32 i32) (result i32)))
    (import "kernel" "sys_async" (func $sys_async (param i32 i32 i32 i32)))

The kernel's two functions. The module name `"kernel"` means they come
from the kernel, not from another object, so `ld` leaves them for the
kernel to supply when the program starts.

- `sys(op, a0, a1, a2)` does operation `op` and returns a number.
- `sys_async(op, token, ptr, len)` starts operation `op` on the `len`
  bytes at address `ptr`. The `token` is any number you choose. It comes
  back with the answer, so you know which request is being answered.

The next import:

    (import "env" "_free" (func $_free (param i32 i32)))

The kernel must put each answer somewhere in the program's memory. So it
calls `_alloc(n)`, which the program exports, to ask for `n` bytes.
`_free(ptr, n)` gives a block back. Both are in `libw.a`, in
[examples/proc.s](examples/proc.s), with `_sig`, which the kernel calls
for a signal. This program calls only `_free`, so only `_free` is
imported. That one call is enough: `ld` takes `proc.o` from `libw.a` for
it, and `_alloc` and `_sig` come along, since they are in the same
object.

### The data

    (memory 1)
    (data $msg (@sym rodata) "hello, world\n")

The same as in `hello2.s`.

    (global $msg_len i32 (i32.const 13))

A **global** is a variable that lives as long as the program. This one
is `i32` and holds 13, the length of the text. It has no `mut`, so it
cannot change.

    (global $done (mut i32) (i32.const 0))

How many bytes are written so far. It starts at 0. `mut` means it can
change.

### $write: ask for a write

    (func $write
      (call $sys_async (i32.const 272) (i32.const 1)
        (i32.add (i32.const (@reloc $msg)) (global.get $done))
        (i32.sub (global.get $msg_len) (global.get $done))))

A function with no parameters and no result. It calls `sys_async` with
four numbers:

1. **`272`, the operation.** Its low byte is the operation, and the
   rest is the operation's argument. Write is 16. Its argument is the
   file descriptor, and stdout is 1. So the number is `16 + 1 × 256`,
   which is `272`. Writing to stderr, descriptor 2, would be
   `16 + 2 × 256 = 528`.
2. **`1`, the token.** Any number would do.
3. **Where the bytes are.** `global.get $done` reads the global. The
   address is where the text starts plus what is written already.
4. **How many.** The length minus what is written already.

The kernel copies the bytes at once. So the program may change them as
soon as `sys_async` returns. The answer comes later.

Why "the rest of the message", and not all of it? A write may be
**short**: the kernel may write fewer bytes than asked. The program
must then write the rest. With 13 bytes this almost never happens, but a
correct program handles it.

### $exit: set the exit status

    (func $exit (param $status i32) (result i32)
      (drop (call $sys (i32.const 1) (local.get $status) (i32.const 0) (i32.const 0)))
      (i32.const 0))

`sys` operation 1 is **Exit**. Its first argument is the exit status.
`local.get $status` reads the parameter. `sys` also takes two more
arguments, which Exit does not use, so they are 0.

`sys` returns a number, and we do not need it. `drop` throws it away. A
function must end with exactly its result on the stack, so a number left
over is an error, and `as` refuses it.

Exit does not stop the program. It only records the status. The program
stops when its step returns 0, so `$exit` returns 0, for the caller to
return in turn. A program that never calls Exit ends with status 1.

### _start: the first step

    (func $_start (export "_start") (param $argv i32) (param $len i32) (result i32)
      (call $write)
      (i32.const 1))

`(export "_start")` makes the function visible to the kernel under that
name. The kernel calls it once, with the arguments at address `$argv`,
`$len` bytes long. `hello` does not use them.

It asks for the write, and returns `1`: "I am waiting for an answer".

### _resume: each answer

    (func $_resume (export "_resume") (param $token i32) (param $reply i32) (param $len i32)
                                      (result i32)

The kernel calls `_resume` with an answer:

- `$token` is the token the request was made with, here 1;
- `$reply` is the address of the answer, in a block from `_alloc`;
- `$len` is its length.

An answer starts with an `i32`, the **status**. For a write, the status
is how many bytes were written. A negative status is an error.

    (local $n i32)

A **local** is a variable that lives only while the function runs.

    (local.set $n (i32.load (local.get $reply)))

`i32.load` reads four bytes from memory at an address, as one `i32`. So
`$n` is the status: the count written.

    (call $_free (local.get $reply) (local.get $len))

The answer is read, so its block goes back.

    (if (i32.le_s (local.get $n) (i32.const 0))
      (then (return (call $exit (i32.const 1)))))

If `$n <= 0`, the write failed. Set exit status 1, and return `$exit`'s
0: the program is done. `le_s` compares as signed, so a negative status
is less than 0.

    (global.set $done (i32.add (global.get $done) (local.get $n)))

Count the bytes that were written.

    (if (i32.lt_u (global.get $done) (global.get $msg_len))
      (then
        (call $write)
        (return (i32.const 1))))

If fewer than 13 bytes are written, this was a short write. Ask to write
the rest, and return 1: wait again. The kernel will call `_resume` once
more.

    (call $exit (i32.const 0))))

Everything is written. Set exit status 0, and return 0: the program is
finished.

## 4. What crt.o does

Now compare the two programs. `hello2.s` has no `_start` or `_resume`.
[examples/crt.s](examples/crt.s) has them, and they are the same as
`hello.s`'s, with one change. Its `_start` does this:

1. It calls `main`.
2. It asks for what `main` printed: `out_buf` returns its address and
   length, which `out_str` had collected.
3. It writes that, as `hello.s` writes its message, short writes and
   all.
4. It exits with the status `main` returned.

So a program on `crt.o` is a plain function. It computes, prints into
memory, and returns. `crt.o` does all the waiting.

`crt.o` is named on the `ld` command line, and not taken from `libw.a`.
`ld` takes an object out of a library only when something needs one of
its names, and no object asks for `_start`.

## 5. Which way to choose

Use `crt.o` for a program that computes, prints and exits, as
[examples/fib.s](examples/fib.s) and [examples/primes.s](examples/primes.s)
do.

Write `_start` and `_resume` yourself for a program that must wait more
than once. [examples/cat.s](examples/cat.s) reads its input piece by
piece, and writes each piece before it reads the next. Its `_resume`
uses the token to tell a read's answer from a write's.

## Where to go next

- [examples/echo.s](examples/echo.s): how the arguments are laid out.
- [examples/cat.s](examples/cat.s): reading, and a program that waits
  many times.
- [README.md](README.md#examples): every call and answer, in one list.
- [Wasm_Assembly_Language.md](Wasm_Assembly_Language.md): the whole
  language `as` reads.
- [Wasm_Bytecode.md](Wasm_Bytecode.md): every instruction.

To see what `as` made of your code, run `disasm hello`. To see its
symbols, run `nm hello.o`.
