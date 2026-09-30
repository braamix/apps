# Programming Braam in assembly

This tutorial explains two small programs, line by line. Both print
`hello, world`. [examples/hello.s](examples/hello.s) does everything
itself, and shows how a Braam program really works.
[examples/hello2.s](examples/hello2.s) is the same program with a
library, `libw.a`, doing the writing.

You need the `wasm` package. Install it, and copy the examples out:

    $ pkg install wasm
    $ cp /pkg/store/wasm-*/share/hello*.s .

Then assemble each program with `as`, and link it with `ld`:

    $ as hello.s
    $ ld hello.o -o hello
    $ ./hello
    hello, world
    $ as hello2.s
    $ ld hello2.o -lw -o hello2
    $ ./hello2
    hello, world

`as hello.s` writes the object `hello.o`, and `ld` makes it the program
`hello`. `hello2` is linked with the library `libw.a` too, which `-lw`
names. `ld` finds it in the package.

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

## 2. hello.s: the whole story

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

### The data

    (memory 1)

The program's memory: one **page**, 64 KiB, at least. The memory is an
array of bytes, and an address is an index into it. `ld` makes this the
memory the kernel gives the process.

    (data $msg (@sym rodata) "hello, world\n")

Thirteen bytes of text, put in memory. `\n` is one byte, a newline.
`(@sym rodata)` lets `ld` choose where they go: it makes the bytes a
*symbol* named `msg`, which is read-only data. We do not know the address
yet; `ld` will. Wherever the program needs the address, it writes
`(@reloc $msg)`, and `ld` fills it in.

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

### _alloc: memory for the kernel

The kernel must put the arguments, and each answer, somewhere in the
program's memory. So it calls the program's `_alloc(n)` to ask for `n`
bytes, and writes them there. `hello` gets one block for the arguments
and one for each answer: a few, so it never gives one back.

    (global $brk (mut i32) (i32.const 0))

Where the next block starts. 0 means "not yet known".

    (func $_alloc (export "_alloc") (param $n i32) (result i32)
      (local $p i32) (local $end i32) (local $top i32)

`_alloc` takes the size and returns the block's address. Three locals:
the block, its end, and the end of memory.

      (local.set $top (i32.shl (memory.size) (i32.const 16)))

`memory.size` is the size of memory in **pages** of 64 KiB. Shifting
left by 16 multiplies by 65536, so `$top` is its size in bytes: the
first address past the end.

      (if (i32.eqz (global.get $brk))
        (then (global.set $brk (local.get $top))))

The first time, blocks start at the end of memory. Below it, `ld` has
put the stack and the data; above it, nothing is in use.

      (local.set $p (global.get $brk))

The block starts where the last one ended.

      (local.set $end (i32.and (i32.add (i32.add (local.get $p) (local.get $n)) (i32.const 3))
                               (i32.const -4)))

Its end, rounded up to a multiple of 4: add 3, then clear the two low
bits (`-4` is all ones but those). So the next block starts where an
`i32` can be read fast.

      (if (i32.gt_u (local.get $end) (local.get $top))
        (then
          (if (i32.eq (memory.grow (i32.shr_u (i32.add (i32.sub (local.get $end) (local.get $top))
                                                       (i32.const 0xffff))
                                              (i32.const 16)))
                      (i32.const -1))
            (then (return (i32.const 0))))))

If the block goes past the end of memory, the memory must grow.
`memory.grow` adds pages, as many as the missing bytes need, rounded up:
add 65535, then shift right by 16 to divide by 65536. It returns the old
size, or `-1` if there is no more memory. Then `_alloc` returns 0, which
tells the kernel it failed.

      (global.set $brk (local.get $end))
      (local.get $p))

The next block will start at this one's end. Return this one.

    (func $_free (export "_free") (param $p i32) (param $n i32))
    (func $_sig (export "_sig") (param $sig i32))

Two functions that do nothing. Every program exports them. The kernel
never calls `_free`; a program would, to give a block back. The kernel
calls `_sig` for a signal the program asked for, and `hello` asks for
none. A function with no body just returns.

`libw.a` has an `_alloc` that gives blocks back:
[examples/proc.s](examples/proc.s). The other examples use it.

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

## 3. hello2.s: with a library

`hello2.s` has a `_start` and a `_resume` shaped as `hello.s`'s, but it
leaves the memory and the writing to `libw.a`. Only what is new is
explained here.

    ;;   as hello2.s
    ;;   ld hello2.o -lw -o hello2

`-lw` links the library `libw.a`. `ld` finds it in the package's `lib/`.

    (import "kernel" "sys" (func $sys (param i32 i32 i32 i32) (result i32)))
    (import "env" "_free" (func $_free (param i32 i32)))
    (import "env" "out_str" (func $out_str (param i32 i32)))
    (import "env" "out_flush" (func $out_flush (param i32) (result i32)))
    (import "env" "out_wrote" (func $out_wrote (param i32) (result i32)))

`sys` is the kernel's, for Exit. The other four are `libw.a`'s. The
module name `"env"` means "another object linked with this one":

- `_free(ptr, n)` gives a block back to the `_alloc` in `libw.a`;
- `out_str(ptr, n)` adds `n` bytes to the output, which is kept in
  memory until it is written;
- `out_flush(fd)` asks the kernel to write the output to `fd`. It
  returns 1 if it asked, and 0 if there was nothing to write;
- `out_wrote(reply)` takes a write's answer. It returns 1 if it had to
  ask for the rest after a short write, 0 if all is written, and -1 if
  the write failed.

`hello2.s` exports no `_alloc`, `_free` or `_sig`. They come from
`libw.a`: `ld` takes the part of the library that defines `_free`, and
the other two are in the same part.

    (func $exit (param $status i32) (result i32)
      (drop (call $sys (i32.const 1) (local.get $status) (i32.const 0) (i32.const 0)))
      (i32.const 0))

The same as in `hello.s`.

    (func $_start (export "_start") (param $argv i32) (param $len i32) (result i32)
      (call $out_str (i32.const (@reloc $msg)) (i32.const 13))

Put the message into the output.

      (if (call $out_flush (i32.const 1))
        (then (return (i32.const 1))))

Ask for the output to be written to stdout, descriptor 1. If a write was
asked for, return 1: wait for its answer.

      (call $exit (i32.const 0)))

Nothing to write: exit with status 0.

    (func $_resume (export "_resume") (param $token i32) (param $reply i32) (param $len i32)
                                      (result i32)
      (local $more i32)
      (local.set $more (call $out_wrote (local.get $reply)))

The answer can only be a write's. `out_wrote` looks at it, and asks for
the rest if the write was short.

      (call $_free (local.get $reply) (local.get $len))

The answer is read, so its block goes back.

      (if (i32.gt_s (local.get $more) (i32.const 0))
        (then (return (i32.const 1))))

If `out_wrote` asked for more, wait again.

      (call $exit (i32.lt_s (local.get $more) (i32.const 0)))))

Otherwise the program is done. `i32.lt_s` gives 1 if `$more` is below 0,
a failed write, and 0 if not: exactly the exit status we want.

## 4. What libw.a does

`libw.a` is a **library**: several objects in one file, which `ar` made.
`ld` takes from it only the objects that define a name the program uses.

- [examples/proc.s](examples/proc.s): `_alloc`, `_free` and `_sig`. Its
  `_alloc` gives blocks back too, if they are freed in the reverse order.
- [examples/fmt.s](examples/fmt.s): the output, kept in memory and then
  written, and numbers in decimal: `out_str`, `out_byte`, `out_u64`,
  `out_nl`, `out_flush`, `out_wrote`, `parse_u64`.
- [examples/args.s](examples/args.s): the arguments, one at a time:
  `argc` and `arg`.

`out_flush` and `out_wrote` are `hello.s`'s `$write` and the second half
of its `_resume`, moved into the library.

## 5. Which way to choose

Every program has its own `_start` and `_resume`: the kernel calls them,
and only the program knows what it is waiting for.

A program that computes, prints and exits looks like `hello2.s`:
[examples/fib.s](examples/fib.s) and [examples/primes.s](examples/primes.s)
compute in a function `$main`, then write what it printed.

A program that must wait more than once keeps track of what it asked
for. [examples/cat.s](examples/cat.s) reads its input piece by piece,
and writes each piece before it reads the next. Its `_resume` uses the
token to tell a read's answer from a write's.

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
