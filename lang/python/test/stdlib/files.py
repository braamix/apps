# open() and the three layers under it, over a real file.
import io
import os
import tempfile

d = tempfile.mkdtemp()
p = os.path.join(d, "a.txt")

with open(p, "w") as f:
    print(type(f).__name__, f.mode, f.encoding, f.writable(), f.readable())
    print(f.write("one\ntwo\n"), end=" ")
    print("three", file=f)
    f.writelines(["four\n", "five"])
print(f.closed)

with open(p) as f:
    print(repr(f.readline()), repr(f.read(3)), repr(f.readline()))
    print(f.tell() > 0, list(f))
with open(p) as f:
    print([line.rstrip() for line in f])
with open(p) as f:
    print(f.readlines(10))
    print(f.read())
with open(p, "rb") as f:
    print(type(f).__name__, f.read(4), f.peek(1)[:1], f.read1(3), f.tell())
    print(f.seek(-2, 2), f.read(), f.seek(0), f.readline())

with open(p, "a") as f:
    print(f.tell(), f.write("\nsix"))
with open(p, "rb+") as f:
    print(type(f).__name__, f.read(3), f.write(b"!"), f.tell())
    f.seek(0)
    print(f.read(8))
    print(f.truncate(5), f.seek(0, 2))
with open(p, "rb") as f:
    print(f.read())

with open(p, "w+") as f:
    f.write("café €\n")
    f.seek(0)
    print(f.read(3), f.tell() == 3, repr(f.read()))
    f.seek(0)
    f.readline()
    print(f.read() == "")

try:
    open(p, "x")
except FileExistsError as e:
    print("x:", e.errno, os.path.basename(e.filename))
try:
    open(os.path.join(d, "nope"))
except FileNotFoundError as e:
    print("missing:", e.errno, e.strerror)
try:
    open(d, "rb")
except IsADirectoryError as e:
    print("dir:", e.errno)

f = open(p, "rb")
f.close()
for op in (f.read, f.tell, lambda: f.seek(0)):
    try:
        op()
    except ValueError as e:
        print("closed:", e)

# Encodings, the newline argument, and errors.
with open(p, "w", encoding="latin-1") as f:
    f.write("über\n")
with open(p, "rb") as f:
    print(f.read())
with open(p, "w", encoding="utf-16") as f:
    f.write("hi\n")
    f.write("there\n")
with open(p, "rb") as f:
    print(f.read())
with open(p, encoding="utf-16") as f:
    print(f.readlines())
with open(p, "w", newline="\r\n") as f:
    f.write("a\nb\n")
with open(p, "rb") as f:
    print(f.read())
with open(p, newline="") as f:
    print(f.readlines())
with open(p) as f:
    print(f.readlines(), f.newlines)
with open(p, "wb") as f:
    f.write(b"ok \xff\xfe end\n")
with open(p, errors="replace") as f:
    print(f.read())
with open(p, errors="surrogateescape") as f:
    print(ascii(f.read()))
try:
    with open(p) as f:
        f.read()
except UnicodeDecodeError as e:
    print("decode:", e.reason, e.start)

# Modes and arguments open() refuses.
for mode in ("rw", "rt+b", "q", "wbb"):
    try:
        open(p, mode)
    except ValueError as e:
        print(mode, e)
try:
    open(p, "rb", encoding="utf-8")
except ValueError as e:
    print(e)
try:
    open(p, "w", buffering=0)
except ValueError as e:
    print(e)

# Unbuffered, and by descriptor.
with open(p, "wb", buffering=0) as f:
    print(type(f).__name__, f.write(b"raw"), f.mode)
fd = os.open(p, os.O_RDONLY)
with open(fd, "rb", closefd=False) as f:
    print(f.read(), f.closefd if hasattr(f, "closefd") else f.raw.closefd)
print(os.read(fd, 10), os.lseek(fd, 0, 0), os.read(fd, 2))
os.close(fd)

with io.FileIO(p, "r") as r:
    print(r.mode, r.readable(), r.writable(), r.seekable(), r.readall())
    print(r.seek(1), r.read(1), r.tell())
    b = bytearray(4)
    r.seek(0)
    print(r.readinto(b), b)

os.remove(p)
os.rmdir(d)
print(os.path.exists(d))
