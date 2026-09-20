# zlib, gzip and bz2, and what stands on them: tarfile, zipfile and shutil's
# archives. Every line is what CPython prints for the same program, so nothing
# here names a compressed size -- those are the library's business and
# identical anyway -- only what came back out.
#
# lzma and zstd are not here: the reference CPython that wrote this golden was
# built without liblzma and libzstd, so there is nothing to compare against.
# test/pycompress.mjs covers those two instead.
import bz2
import gzip
import io
import os
import shutil
import tarfile
import zipfile
import zlib


def say(*args, end="\n"):
    print(*args, end=end, flush=True)


BODY = b"the quick brown fox jumps over the lazy dog\n" * 200

# --- zlib
say(zlib.crc32(b"hello"), zlib.adler32(b"hello"))
say(zlib.crc32(b"lo", zlib.crc32(b"hel")), zlib.crc32(b"hello"))
say(zlib.crc32_combine(zlib.crc32(b"hel"), zlib.crc32(b"lo"), 2) == zlib.crc32(b"hello"))
say(zlib.decompress(zlib.compress(BODY)) == BODY)
say(zlib.decompress(zlib.compress(BODY, 9, -15), -15) == BODY)
say(zlib.decompress(zlib.compress(BODY, wbits=31), 47) == BODY)
c = zlib.compressobj(6, zlib.DEFLATED, -15)
d = zlib.decompressobj(-15)
say(d.decompress(c.compress(BODY) + c.flush()) == BODY, d.eof, d.unused_data)
d = zlib.decompressobj()
blob = zlib.compress(BODY)
say(len(d.decompress(blob, 50)), d.eof, len(d.unconsumed_tail) > 0)
say(len(d.flush()), d.eof)
say(zlib.ZLIB_VERSION == zlib.ZLIB_RUNTIME_VERSION, len(zlib.ZLIB_VERSION_INFO))

# --- gzip
say(gzip.decompress(gzip.compress(BODY)) == BODY)
with gzip.open("/tmp/z.gz", "wb") as f:
    f.write(BODY)
with gzip.open("/tmp/z.gz", "rb") as f:
    say(f.read() == BODY)
with gzip.open("/tmp/z.gz", "rt") as f:
    say(len(f.read()))
bio = io.BytesIO()
with gzip.GzipFile(fileobj=bio, mode="wb", mtime=0) as f:
    f.write(BODY)
say(gzip.decompress(bio.getvalue()) == BODY)

# --- bz2
say(bz2.decompress(bz2.compress(BODY)) == BODY)
say(bz2.decompress(bz2.compress(BODY, 1) + bz2.compress(BODY, 1)) == BODY + BODY)
dc = bz2.BZ2Decompressor()
say(len(dc.decompress(bz2.compress(BODY), 40)), dc.eof, dc.needs_input)
say(len(dc.decompress(b"")), dc.eof)
with bz2.open("/tmp/z.bz2", "wb") as f:
    f.write(BODY)
with bz2.open("/tmp/z.bz2", "rb") as f:
    say(f.read() == BODY)

# --- zipfile, over every method the codecs allow
os.makedirs("/tmp/tree/sub", exist_ok=True)
with open("/tmp/tree/a.txt", "wb") as f:
    f.write(BODY)
with open("/tmp/tree/sub/b.txt", "wb") as f:
    f.write(BODY[:100])
for how, name in ((zipfile.ZIP_STORED, "stored"),
                  (zipfile.ZIP_DEFLATED, "deflated"),
                  (zipfile.ZIP_BZIP2, "bzip2")):
    with zipfile.ZipFile("/tmp/z.zip", "w", how) as z:
        z.writestr("a.txt", BODY)
    with zipfile.ZipFile("/tmp/z.zip") as z:
        say(name, z.read("a.txt") == BODY, z.namelist())

# --- tarfile and shutil
for suffix, mode in (("", "w"), (".gz", "w:gz"), (".bz2", "w:bz2")):
    with tarfile.open("/tmp/t.tar" + suffix, mode) as t:
        t.add("/tmp/tree/a.txt", arcname="a.txt")
    with tarfile.open("/tmp/t.tar" + suffix) as t:
        say(suffix or "plain", sorted(t.getnames()),
            t.extractfile("a.txt").read() == BODY)

# Not "zip": a directory in the store has no mtime, and zipfile refuses a
# timestamp before 1980. test/pycompress.mjs pins that.
for fmt, ext in (("tar", ".tar"), ("gztar", ".tar.gz"), ("bztar", ".tar.bz2")):
    made = shutil.make_archive("/tmp/arc-" + fmt, fmt, "/tmp/tree")
    say(fmt, os.path.basename(made) == "arc-" + fmt + ext)
    shutil.unpack_archive(made, "/tmp/out-" + fmt)
    say("  ", sorted(os.listdir("/tmp/out-" + fmt)))
    with open("/tmp/out-" + fmt + "/a.txt", "rb") as f:
        say("  ", f.read() == BODY)
