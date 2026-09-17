# Python classes under the native layers: RawIOBase and friends subclassed.
import io

class Chunky(io.RawIOBase):
    """Hands out at most three bytes a call, and logs what it was asked."""

    def __init__(self, data):
        self.data = bytearray(data)
        self.pos = 0
        self.log = []

    def readable(self):
        return True

    def writable(self):
        return True

    def seekable(self):
        return True

    def readinto(self, b):
        self.log.append(("readinto", len(b)))
        n = min(3, len(b), len(self.data) - self.pos)
        b[:n] = self.data[self.pos:self.pos + n]
        self.pos += n
        return n

    def write(self, b):
        b = bytes(b)[:4]
        self.log.append(("write", len(b)))
        self.data[self.pos:self.pos + len(b)] = b
        self.pos += len(b)
        return len(b)

    def seek(self, off, whence=0):
        self.pos = [off, self.pos + off, len(self.data) + off][whence]
        return self.pos

    def tell(self):
        return self.pos


c = Chunky(b"hello world\nsecond line\n")
print(c.read(5), c.read(), c.readall(), c.readline())
c.seek(0)
print(c.readlines(), len(c.log))

r = io.BufferedReader(Chunky(b"0123456789" * 3), buffer_size=8)
print(r.read(4), r.peek(2)[:2], r.read1(20), r.read(), r.raw.log[:4])
r = io.BufferedReader(Chunky(b"line1\nline2\nline3"), buffer_size=4)
print(r.readline(), list(r))

raw = Chunky(b"")
w = io.BufferedWriter(raw, buffer_size=5)
print(w.write(b"abc"), w.write(b"defghijk"), raw.log)
w.flush()
print(bytes(raw.data), raw.log)

rw = io.BufferedRandom(Chunky(b"abcdefghij"), buffer_size=4)
print(rw.read(2), rw.write(b"XY"), rw.tell(), rw.seek(0), rw.read())
rw.flush()
print(bytes(rw.raw.data))

t = io.TextIOWrapper(io.BufferedReader(Chunky("naïve\nçà\n".encode())), encoding="utf-8")
print(t.readlines())

class Upper(io.TextIOBase):
    def __init__(self):
        self.parts = []

    def write(self, s):
        self.parts.append(s.upper())
        return len(s)

u = Upper()
print("printed", "here", file=u)
u.writelines(["a", "b"])
print(u.parts, u.readable(), u.closed)
try:
    u.read()
except io.UnsupportedOperation as e:
    print(type(e).__name__, e)

class Nothing(io.RawIOBase):
    pass

n = Nothing()
for op in (lambda: n.read(1), lambda: n.write(b"x"), n.fileno, lambda: n.seek(0), n.tell):
    try:
        op()
    except (OSError, NotImplementedError) as e:
        print(type(e).__name__, e)
print(n.readable(), n.writable(), n.seekable(), n.isatty(), n.closed)
with n:
    pass
print(n.closed)
try:
    n.flush()
except ValueError as e:
    print(e)

class Closing(io.BytesIO):
    def close(self):
        print("closing", self.getvalue())
        super().close()

with Closing(b"bytes") as cl:
    pass
print(cl.closed, isinstance(cl, io.IOBase), issubclass(io.FileIO, io.RawIOBase))
print(io.BufferedIOBase.__mro__[1].__name__, io.TextIOWrapper.__mro__[1].__name__)

pair = io.BufferedRWPair(io.BytesIO(b"in"), io.BytesIO())
print(pair.read(), pair.write(b"out"), pair.readable(), pair.writable())
pair.close()
print(pair.closed)
