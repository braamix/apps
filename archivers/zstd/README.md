# zstd (Zstandard 1.6.0)

Upstream CLI from [Zstandard](https://github.com/facebook/zstd) 1.6.0
(`programs/`), ported like [archivers/xz](../xz/): `PORT`, Group B `b_*` I/O,
libzstd from the SDK as `braam::zstd`, zlib and liblzma through
`braam::compat`. One wasm binary provides `zstd`, `unzstd`, `zstdcat` and
`zstdmt`.

The four bin links are the four names upstream's `exeNameMatch()` presets
recognise that are not already taken: `gzip`, `gunzip`, `gzcat` and `zcat`
belong to [archivers/gzip](../gzip/), and `xz`, `unxz`, `lzma` and `unlzma` to
[archivers/xz](../xz/). The preset code for all of them is still here and would
fire if those links existed; `--format=` is how the other formats are reached.

The compressor is libzstd itself, so a frame this writes is upstream's frame,
and [test/frames.mjs](test/frames.mjs) asserts it: eight encodings — levels 1,
3 and 19, `--fast=3`, `--long=20`, `--no-check`, `--format=xz` and
`--format=lzma` — are byte for byte what upstream's own binary writes for the
same input, and the goldens beside it came out of that binary.
`--format=gzip` is the ninth and is not among them, because it differs in
exactly one byte: the OS field of the gzip header, which is zlib's `OS_CODE`
and not this port's. So a frame that changes there says the CLI passed a
different parameter, not that the encoder drifted.

## What is not here

lz4 is the one of upstream's four optional formats that is absent, because the
SDK vendors zlib, liblzma and libzstd and not liblz4. An lz4 frame is still
recognised by its magic and named as such rather than reported as an unknown
header, which is what upstream's own no-lz4 build does.

`ZSTD_NODICT`, `ZSTD_NOBENCH` and `ZSTD_NOTRACE` are set: the dictionary
builder wants `ZDICT_*`, which `braam::zstd` does not export; the benchmark
wants a clock that measures CPU time; and the trace log wants `_POSIX_VERSION`.
`--train`, `-b` and `--trace` are gone from `--help` with them, and so is
`--priority=rt`.

libzstd here is single-threaded, so `ZSTD_MULTITHREAD` is undefined and
`FIO_setNbWorkers` prints upstream's `Note : multi-threading is disabled` on
every compression at the default verbosity — which is what upstream's own
single-threaded build does. `-q` silences it.

## Memory

Braam caps a process at 100 MiB and the window is what spends it: a
compressing stream of unknown size is 3.5 MiB at level 3 and 89.5 MiB at 19, so
19 is the highest level that fits and `--ultra`'s 20 through 22 are not usable.
A known input size shrinks both, so `zstd -20 file` can succeed where
`zstd -20 < file` cannot. Decompression is 94 KiB plus the window the frame
asks for.

## What changed

| Upstream | Braam |
| --- | --- |
| `main` / `exit` | `zstd_main` rebuilds `argv` from `Args`; `co_return` status |
| `EXM_THROW` (message + `exit`) | `EXM_ERROR` / `CO_ERROR_IF`: message, sticky `zstd_fail`, `return` |
| `errorOut` in the argument parsers | `zstd_fail`; the loop's end checks `zstd_fatal()` |
| `fread`/`fwrite`/`fopen`/`stat`/`opendir` | `co_await b_*`, and `zstd_fread`/`zstd_fwrite` around the two hot ones |
| `signal(SIGINT, INThandler)` | `sig_catch`; `g_artefact` is unlinked in `proc_main` |
| `DISPLAY` / `DISPLAYOUT` `fprintf` | `zstd_display` into a growing buffer, flushed where the process parks |
| `clock()` / `CLOCKS_PER_SEC` | `proc_now()`; the cpu-load figure therefore reads 100% |
| `mmap` for `--mmap-dict` | `malloc` and `free`, which is upstream's own fallback |
| `fileio_asyncio.c`'s thread pools | The synchronous path only; `AIO_supported()` is 0 |
| `UTIL_countPhysicalCores` | 1 |
| `chmod` / `utime` / `chown` | No-op |

`DISPLAY` is the one that shaped `braam.cpp`. It is called from plain
non-coroutine functions, which cannot await a write, so it formats into a heap
buffer that every `co_await`-ing primitive drains first. The buffer has to grow
rather than truncate: `--help` is twelve kilobytes written by a hundred
`DISPLAYOUT` calls with no suspension point between the first and the last.

`zstd_fail` is the other. Upstream's error path is `exit()` from arbitrary
depth, and there is no `setjmp` here to stand in for it, so a failure records a
status and the caller returns one frame at a time. `zstd_fread` and
`zstd_fwrite` then refuse every later read and write, which is what ends a loop
whose body does not check — and it is also where `^C` is collected, since a
signal is delivered where the process parks and a read is where it parks.

## Build

From the repo root: `make` builds `build/archivers/zstd/zstd.wasm`. Tests:
`make test TESTS=archivers/zstd/test/roundtrip.mjs` (and siblings in
`archivers/zstd/test/`).
