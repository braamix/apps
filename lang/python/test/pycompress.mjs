// lzma and zstd, and the three places compression differs from CPython.
//
// test/stdlib/zips.py holds zlib, gzip and bz2 against CPython byte for byte;
// these two cannot go there, because the reference interpreter that writes
// those goldens was built without liblzma and libzstd. So this file states
// what each should print and checks it, the way pyio.mjs does.
//
// The three differences, all of them the platform showing through:
//
//   - **lzma's high presets are what the process can spare.** Nothing is
//     refused in advance: liblzma is asked, and only what it will not build is
//     a MemoryError. Preset 6 wants 94 MiB of a 100 MB process, so it works
//     when it is the first thing a program does and not once the program holds
//     much else; 7 to 9 want 185, 369 and 673 MiB and never fit. A preset the
//     caller named and that fails is reported, because writing something other
//     than what was asked for would be a lie; the default nobody named comes
//     down until it fits, so lzma.compress(data) always works. The two checks
//     below are the same preset 6 succeeding and failing, which is the point.
//   - **zstd's dictionaries are untrained.** zdict.h, which holds zstd's COVER
//     trainer, is not in braam::zstd, so train_dict and finalize_dict build a
//     content-only dictionary: the tail of the samples, which is what a raw
//     dictionary is. It works and it helps, but it has no entropy tables and
//     so no dict_id.
//   - **A directory has no mtime**, so shutil's zip archiver refuses it.

import { boot, script, same, ok } from "./pylib.mjs";

await boot("pycompress");
let bad = 0;
const check = (what, got, want) => {
    if (!same(what, got, want)) bad++;
};

const BODY = 'b"the quick brown fox jumps over the lazy dog\\n" * 200';

// ------------------------------------------------------------------ lzma

check("lzma round trips", script(`
import lzma
BODY = ${BODY}
print(lzma.decompress(lzma.compress(BODY)) == BODY)
print(lzma.decompress(lzma.compress(BODY, preset=1)) == BODY)
print(lzma.decompress(lzma.compress(BODY, format=lzma.FORMAT_ALONE, preset=1),
                      format=lzma.FORMAT_ALONE) == BODY)
raw = [{"id": lzma.FILTER_LZMA2, "preset": 1}]
c = lzma.LZMACompressor(format=lzma.FORMAT_RAW, filters=raw)
d = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=raw)
print(d.decompress(c.compress(BODY) + c.flush()) == BODY)
with lzma.open("/tmp/z.xz", "wb", preset=1) as f:
    f.write(BODY)
with lzma.open("/tmp/z.xz", "rb") as f:
    print(f.read() == BODY)
`).out, "True\nTrue\nTrue\nTrue\nTrue\n");

check("lzma filter properties", script(`
import lzma
print(lzma._encode_filter_properties({"id": lzma.FILTER_LZMA1, "pb": 2, "lp": 0,
                                      "lc": 3, "dict_size": 8 << 20}))
spec = lzma._decode_filter_properties(lzma.FILTER_LZMA1, b"]\\x00\\x00\\x80\\x00")
print(spec["id"] == lzma.FILTER_LZMA1, spec["pb"], spec["lp"], spec["lc"], spec["dict_size"])
print(lzma._decode_filter_properties(lzma.FILTER_X86, b""))
print(lzma.is_check_supported(lzma.CHECK_NONE), lzma.is_check_supported(lzma.CHECK_CRC32))
print(lzma.LZMA_VERSION, lzma.LZMA_VERSION_INFO.stability)
`).out,
      "b']\\x00\\x00\\x80\\x00'\n" +
      "True 2 0 3 8388608\n" +
      "{'id': 4}\n" +
      "True True\n" +
      "5.8.4 stable\n");

// The biggest preset is the first thing this program does, so it has the
// whole process to itself and liblzma builds it.
check("lzma's default preset, with room", script(`
import lzma
d = b"x" * 5000
b = lzma.compress(d, preset=lzma.PRESET_DEFAULT)
print("preset 6", lzma.decompress(b) == d)
`).out, "preset 6 True\n");

// Nothing is refused in advance, so the same preset that worked above is
// refused once three encoders before it have taken the room. That is the
// allocator's answer and not a rule of this module's.

// Asked for one at a time in a process that already holds a little, the low
// presets fit and the top three never do -- 185, 369 and 673 MiB.
check("lzma's presets", script(`
import lzma
d = b"x" * 5000
for p in (0, 2, 4, 6, 9):
    try:
        b = lzma.compress(d, preset=p)
        print(p, "ok", lzma.decompress(b) == d)
    except MemoryError as e:
        print(p, "refused")
for i in range(8):
    blob = lzma.compress(d)
print("default eight times", lzma.decompress(blob) == d)
`).out,
      "0 ok True\n2 ok True\n4 ok True\n6 refused\n9 refused\n" +
      "default eight times True\n");

check("lzma's message names what it wanted", script(`
import lzma
try:
    lzma.compress(b"x", preset=9)
except MemoryError as e:
    print(str(e).startswith("LZMACompressor wanted 673 MiB"), str(e).endswith("lower preset"))
`).out, "True True\n");

// ------------------------------------------------------------------ zstd

check("zstd round trips", script(`
from compression import zstd
import io
BODY = ${BODY}
print(zstd.decompress(zstd.compress(BODY)) == BODY)
print(zstd.decompress(zstd.compress(BODY, 10)) == BODY)
c = zstd.ZstdCompressor()
print(zstd.decompress(c.compress(BODY) + c.flush()) == BODY)
bio = io.BytesIO()
with zstd.ZstdFile(bio, "wb") as f:
    f.write(BODY)
print(zstd.decompress(bio.getvalue()) == BODY)
print(zstd.COMPRESSION_LEVEL_DEFAULT, zstd.zstd_version)
`).out, "True\nTrue\nTrue\nTrue\n3 1.6.0\n");

check("zstd frames and max_length", script(`
from compression import zstd
BODY = ${BODY}
frame = zstd.compress(BODY)
info = zstd.get_frame_info(frame)
print(info.decompressed_size, info.dictionary_id)
print(zstd.get_frame_size(frame) == len(frame))
d = zstd.ZstdDecompressor()
print(len(d.decompress(frame, 40)), d.eof, d.needs_input)
print(len(d.decompress(b"")), d.eof, d.unused_data)
`).out, "8800 0\nTrue\n40 False False\n8760 True b''\n");

check("zstd dictionaries", script(`
from compression import zstd
BODY = ${BODY}
d = zstd.ZstdDict(b"the quick brown fox " * 8, is_raw=True)
print(len(d), d.dict_id)
print(zstd.decompress(zstd.compress(BODY, zstd_dict=d), zstd_dict=d) == BODY)
print(zstd.decompress(zstd.compress(BODY, zstd_dict=d.as_prefix),
                      zstd_dict=d.as_prefix) == BODY)
`).out, "160 0\nTrue\nTrue\n");

// A content-only dictionary: no id, but it really is used and it really does
// help, which is the whole reason for building one.
check("zstd's untrained dictionaries", script(`
from compression import zstd
samples = [b"the quick brown fox %d jumps over the lazy dog\\n" % i for i in range(40)]
d = zstd.train_dict(samples, 1024)
body = samples[7] * 30
print(len(d.dict_content), d.dict_id)
print(zstd.decompress(zstd.compress(body, zstd_dict=d), zstd_dict=d) == body)
print(len(zstd.compress(body, zstd_dict=d)) < len(zstd.compress(body)))
f = zstd.finalize_dict(d, samples, 1024, 3)
print(len(f.dict_content), f.dict_id)
print(zstd.decompress(zstd.compress(body, zstd_dict=f), zstd_dict=f) == body)
`).out, "1024 0\nTrue\nTrue\n1024 0\nTrue\n");

// There are no threads, so a worker count is the one parameter that is fixed.
check("zstd has no workers", script(`
from compression import zstd
print(zstd.compress(b"x" * 100, options={zstd.CompressionParameter.nb_workers: 0}) != b"")
try:
    zstd.compress(b"x" * 100, options={zstd.CompressionParameter.nb_workers: 2})
except ValueError as e:
    print("ValueError", "threads" in str(e))
`).out, "True\nValueError True\n");

// ------------------------------------------------------ what a zip refuses

check("a directory has no mtime", script(`
import os, shutil, zipfile
os.makedirs("/tmp/tree/sub", exist_ok=True)
with open("/tmp/tree/a.txt", "wb") as f:
    f.write(b"body")
print(os.stat("/tmp/tree/sub").st_mtime, os.stat("/tmp/tree/a.txt").st_mtime > 0)
try:
    shutil.make_archive("/tmp/arc", "zip", "/tmp/tree")
except ValueError as e:
    print("ValueError", str(e))
# A zip made by hand, with no directory entry in it, is fine.
with zipfile.ZipFile("/tmp/z.zip", "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("a.txt", b"body")
with zipfile.ZipFile("/tmp/z.zip") as z:
    print(z.read("a.txt"))
`).out,
      "0.0 True\n" +
      "ValueError ZIP does not support timestamps before 1980\n" +
      "b'body'\n");

// zipfile's other methods, which the four codecs decide. ZIP_LZMA is the one
// that does not fit: zipfile hands _lzma a filter chain naming an 8 MiB
// dictionary, which wants 94 MiB, and by then the module's own imports have
// taken enough that the process has not got it. A chain the caller spelled
// out is reported rather than quietly shrunk, so this is a MemoryError.
check("zipfile's methods", script(`
import zipfile
BODY = ${BODY}
for how, name in ((zipfile.ZIP_STORED, "stored"),
                  (zipfile.ZIP_DEFLATED, "deflated"),
                  (zipfile.ZIP_BZIP2, "bzip2"),
                  (zipfile.ZIP_LZMA, "lzma"),
                  (zipfile.ZIP_ZSTANDARD, "zstandard")):
    try:
        with zipfile.ZipFile("/tmp/z.zip", "w", how) as z:
            z.writestr("a.txt", BODY)
        with zipfile.ZipFile("/tmp/z.zip") as z:
            print(name, z.read("a.txt") == BODY)
    except MemoryError:
        print(name, "refused")
`).out,
      "stored True\ndeflated True\nbzip2 True\nlzma refused\nzstandard True\n");

if (bad) {
    console.error(`\npycompress: ${bad} checks failed`);
    process.exit(1);
}
ok("lzma and zstd round trip, and the three differences hold");
