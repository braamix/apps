# iconv — character set conversion

Converts text between character encodings. About two hundred of them, from
ASCII and the ISO 8859 family through KOI8 and the Windows code pages to the
CJK multibyte sets and the whole UTF family.

**This is Apple's libiconv, which is FreeBSD's, which is the NetBSD Citrus
implementation.** Citrus was written by Tsuneo Nozaki and the Citrus Project
around 2003 and is the iconv the BSDs ship; Apple's fork adds transliteration
and a `wchar_t` conversion path. The port is of Apple's tree, and the licence —
three BSD variants — is in [LICENSE](LICENSE).

## Using it

    iconv [-cs] [-f <from_code>] [-t <to_code>] [file ...]
    iconv -l
    iconv -h

`-f` names the encoding the input is in and `-t` the one to produce; either may
be left out and defaults to UTF-8. `-c` drops characters the target cannot
represent instead of failing, `-s` suppresses the count of them, and `-l` lists
every encoding name the tables know. `//TRANSLIT` after a target name asks for
an approximation where there is no exact mapping, and `//IGNORE` is `-c` by
another spelling. `-h`, or `--help`, prints all of that as an option list.

    iconv -f KOI8-R -t UTF-8 old.txt > new.txt

Braam holds text as Unicode and its terminal renders UTF-8, so this is how text
from anywhere else gets in — a file fetched from an old archive, a listing
written on a Russian or Japanese machine, a CSV out of a Windows tool.

**The console is UTF-8 and only UTF-8.** The grid stores codepoints rather than
bytes, so `iconv -t SHIFT_JIS` onto a terminal is mojibake by construction.
The command says so once on stderr and converts anyway; redirect to a file.

## What the port changed

No libc: no stdio, malloc, dlopen, mmap, locale or errno, and a program is a
coroutine rather than a `main`. Citrus is also C, and this is C++.

| Upstream | Here |
| --- | --- |
| `dlopen` of `/usr/lib/i18n/lib<module>.dylib` | a static table of 25 modules, linked in ([citrus_module.cpp](citrus_module.cpp)) |
| `mmap` of a table file | the file read into a heap block ([citrus_mmap.cpp](citrus_mmap.cpp)) |
| `_PATH_ESDB` at `/usr/share/i18n` | a suffix, with the prefix found at startup ([citrus_paths.cpp](citrus_paths.cpp)) |
| `pthread_rwlock` around three caches | nothing; one thread |
| `nl_langinfo`, `locale_charset` | `"UTF-8"`, the only locale there is |
| `malloc`, `str*`, `snprintf`, `qsort` | [braam.cpp](braam.cpp), over `heap_alloc` |
| `mbrtowc`, `wcrtomb` | the kernel's UTF-8 codec — `wchar_t` is UTF-32 here |
| `getopt_long` | a hand-rolled parser; `OptParse` has no long options |
| `stdio` | `proc/file.h`'s `File`, on its byte path |
| `mkcsmapper`, `mkesdb` (lex + yacc) | [mkcsmapper.py](mkcsmapper.py), [mkesdb.py](mkesdb.py) |

Structure:

- **Only opening a conversion reads a file.** Citrus consults its databases in
  `iconv_open` and never again, so the coroutine boundary is exact: about
  twenty functions on the open path became `Task<int>`, and the entire
  conversion path — all seventeen encoding modules, all five mappers, and
  `iconv_std`'s converter — is untouched synchronous C. Two vtable slots moved
  with it, `mo_init` and `io_init_shared`.

- **The index files are read once at startup.** `esdb.dir`, `esdb.alias`,
  `mapper.dir` and `charset.pivot` are consulted whatever the conversion, so
  they are loaded before anything else and `_citrus_map_file` answers out of
  that, keeping upstream's synchronous signature. That is what lets
  `citrus_lookup.c`, the pivot search and `iconv -l` stay ordinary functions.
  Only a `.esdb`, `.mps` or `.646` — which depend on the encodings asked
  for — is read through the awaiting half.

- **`PATH_MAX` is 256, not 1024.** Citrus builds paths in stack buffers, and a
  coroutine's locals live in a heap frame that must stay under 512 bytes. The
  longest path the library ever builds measures 81 bytes.

- **Upstream's Apple branches are the ones selected**, because the data we
  generate is theirs: every `.mps` carries the transliteration table they read.
  They are chosen with a `-D_CITRUS_APPLE` of our own rather than by claiming
  to be a platform this is not.

- **The tables are built from source, not shipped compiled.** `mkcsmapper` and
  `mkesdb` are lex+yacc that link against libiconv, so they cannot be built
  here; [mki18n.py](mki18n.py) and its three helpers compile the 512 checked-in
  `.src` files instead. The output is byte-identical to upstream's — 423 `.mps`,
  241 `.esdb` and four index containers — and [verify.py](verify.py) is what
  says so.

- **The data ships in the package with the binary.** It lands at
  `/pkg/store/iconv-<version>/share/i18n/`, a path carrying a version the
  binary does not know, so at startup it reads the link `PATH` found —
  `/pkg/bin/iconv` — and takes the directory of its directory. Upstream's own
  `libiconv_set_relocation_prefix` is the hook for exactly this. `$ICONV_PREFIX`
  overrides it, which is how the tests run without installing anything.

## Differences from upstream worth knowing

- **The `wchar_t` conversion path works, and did not have to.** The plan was to
  cut it, but the locale here is UTF-8 and `wchar_t` is UTF-32, so
  `mbrtowc`/`wcrtomb` are forty lines over `kernel/text.h`.

- **`iconv -l` buffers its output.** `iconvlist` walks the list through a plain
  callback, which cannot `co_await`; the names accumulate and go out in one
  write.

- **A missing input file is a warning, not a stop** — Apple's conformance
  branch, kept. The operands after it are still converted and the status is 1.

- **Citrus and GNU disagree about some CJK mappings**, mostly the user-defined
  and vendor-defined rows. This is upstream's own position rather than
  something introduced here: its `tests/iconv/tablegen/cmp.sh` prints `DIFFER`
  and then exits 0 whatever it found. [test/convert.mjs](test/convert.mjs)
  asserts exact agreement for the twenty-five encodings where the two agree and
  reports the rest.

- **`-h` and `--help` are new here.** Upstream has only the usage, printed on
  an error, and it lists the flag combinations without saying what any flag
  does. The help explains the options, the `//TRANSLIT` and `//IGNORE`
  suffixes and the long forms; the usage on an error is two lines and points
  at it.

- **Renames forced by C++**: none of the identifiers, but a `void *` that C
  converts on its own now says so, six module headers declare their ops
  `extern` rather than tentatively defining them, and an anonymous enum inside
  a struct moved out of it.

## Building and packaging

    make            # iconv.wasm, and the tables beside it
    make package    # iconv-1.16-r1.zip

The package is about 1.9 MB and unpacks to 42 MiB, which needs a Braam whose
`pkg` allows it — `UNPACK_MAX` rose to 50 MiB in braam-core for this. The zip
holds `bin/iconv` and 700 files under `share/i18n/`; only the flat `bin/` entry
becomes a command.

## Testing

    make test

- [test/smoke.mjs](test/smoke.mjs) — UTF-8 to KOI8-R and back, and `iconv -l`,
  with only the thirteen tables that conversion touches planted.
- [test/convert.mjs](test/convert.mjs) — the tables against GNU libiconv's
  answers, from the reference corpus upstream ships in
  `tests/iconv/ref/`: 137,385 mappings over twenty-five encodings, both
  directions. It needs the upstream tree, which is gitignored, and says so and
  passes over without it.
- [test/errors.mjs](test/errors.mjs) — the exit statuses and messages: an
  unknown encoding, an illegal sequence with and without `-c`, a missing
  operand, a truncated character, `-l` with another flag, and the long options.

## Files

| | |
| --- | --- |
| `iconv.cpp` | upstream `iconv/iconv.c`, the command |
| `braam.h`, `braam.cpp` | the C library citrus calls, over the kernel's |
| `citrus_module.cpp` | the static module table, replacing `citrus_module.c` |
| `citrus_mmap.cpp` | whole-file reads and the index cache, replacing `citrus_mmap.c` |
| `citrus_paths.cpp` | where `/share/i18n` is |
| `bsd_iconv.cpp` | upstream `citrus/bsd_iconv.c`, the public API |
| `citrus_iconv.cpp` | upstream `citrus/citrus_iconv.c`, the handle cache |
| `citrus_esdb.cpp` | upstream `citrus/citrus_esdb.c`, encoding schemes |
| `citrus_csmapper.cpp` | upstream `citrus/citrus_csmapper.c`, charset mapping and the pivot |
| `citrus_mapper.cpp` | upstream `citrus/citrus_mapper.c`, the mapper cache |
| `citrus_stdenc.cpp` | upstream `citrus/citrus_stdenc.c`, the encoding vtable |
| `citrus_lookup.cpp`, `citrus_db.cpp` | upstream, the database readers |
| `citrus_memstream.cpp`, `citrus_prop.cpp`, `citrus_hash.cpp`, `citrus_bcs*.cpp` | upstream, unchanged but for casts |
| `citrus_none.cpp` | upstream, the built-in `NONE` encoding |
| `modules/` | upstream `libiconv_modules/`: 2 drivers, 17 encodings, 5 mappers |
| `include/` | the freestanding headers citrus's `#include`s name |
| `data/` | upstream's `.src` sources, 512 files |
| `mkcsmapper.py`, `mkesdb.py`, `mkdb.py`, `citrusdb.py`, `mki18n.py` | the table compilers |
| `verify.py` | byte-identity against upstream's prebuilt tree |
