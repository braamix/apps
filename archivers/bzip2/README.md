# bzip2 (1.0.8)

Julian Seward's bzip2 for Braam: compress and decompress `.bz2` files, test
integrity, and read or write concatenated streams.

## Using it

After `pkg install bzip2`, commands `bzip2`, `bunzip2`, and `bzcat` are the
same binary. Compress with `bzip2 file`; decompress with `bzip2 -d` or
`bunzip2`.

## What changed

Same shape as [archivers/gzip](../gzip/): `PORT`, Group B `b_*` I/O, codec in
the SDK's `braam::bzip2` via `<bzlib.h>`. Upstream `bzip2.c` is ported in
`bzip2.cpp`; the driver is `braam.cpp`.

| Upstream | Braam |
| --- | --- |
| `main` / `exit` | `bzip2_main` / `co_return` status |
| `BZ2_bzRead*` / `BZ2_bzWrite*` | `BZ2_bzDecompress*` / `BZ2_bzCompress*` on `bz_stream` |
| `fopen` / `fread` / … | `co_await b_*` |
| `utime` / `fchmod` / `fchown` | No-op |
| `signal()` | `sig_catch(SIG_INT)`; ^C unlinks in-progress output |
| `fopen_output_safely` | `b_open(O_EXCL)` + `b_fdopen` |

## Build and test

`make` builds `build/archivers/bzip2/bzip2.wasm`. Tests:
`archivers/bzip2/test/*.mjs` (see top-level `make test`).
