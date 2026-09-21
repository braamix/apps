# xz (XZ Utils 5.8.4)

Upstream CLI from [XZ Utils](https://tukaani.org/xz/) 5.8.4, ported like
[archivers/bzip2](../bzip2/): `PORT`, Group B `b_*` I/O, liblzma from the SDK
(`<lzma.h>` via `braam::compat`). One wasm binary provides `xz`, `unxz`,
`xzcat`, `lzma`, `unlzma`, and `lzcat`.

## Memory

Braam caps a process at 100 MiB. `hardware_init()` treats that as total RAM
(`lzma_physmem()` is 0). Default compression memory limit leaves ~20 MiB
headroom so a bare `xz file` can auto-adjust off preset 6 instead of trapping.
Presets 7–9 are not usable here. Multi-threaded encoding is not linked; `-T` above
1 is clamped with a warning.

## What changed

| Upstream | Braam |
| --- | --- |
| `main` / `exit` | `xz_main` / `co_return` status |
| `argc`/`argv` | Braam `Args` views; `getopt_long_args` (not a copied argv) |
| `getopt_long` | Minimal `getopt.cpp` (env vars only; not GNU getopt) |
| `read`/`write`/`lseek` | `co_await b_*` |
| `fopen` for `--files` | Deferred `co_await b_fopen` in `main.cpp` |
| `lzma_stream_encoder_mt` | Not linked; `-T` clamped to 1 |
| `utime` / sparse / sandbox | No-op / omitted |
| SIGINT + dest | `sig_catch` + unlink in-progress output |
| `--list` | `lzma_file_info_decoder` over `io_pread` / `io_seek_src` |
| gettext / tuklib_mbstr | English; `_()` is identity |
| `--help` / `--long-help` | Structured usage on stderr (`-H` lists advanced options) |
| `message_fatal` / `tuklib_exit` | Sticky exit status, no `exit()` |

## Build

From the repo root: `make` builds `build/archivers/xz/xz.wasm`. Tests:
`make test TESTS=archivers/xz/test/roundtrip.mjs` (and siblings in
`archivers/xz/test/`).
