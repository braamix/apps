# BytesIO, StringIO and IncrementalNewlineDecoder: streams over memory.
import io

b = io.BytesIO(b"hello\nworld\n")
print(b.read(3), b.tell(), b.readline(), b.readlines())
print(b.seek(0), b.read1(2), b.peek()[:3] if hasattr(b, "peek") else None)
print(b.seek(20), b.write(b"!"), b.getvalue())
print(b.seek(-3, 2), b.read(), b.seek(0, 1))
print(b.truncate(4), b.getvalue(), b.tell())
v = b.getbuffer()
v[0] = ord("J")
print(b.getvalue(), len(v))
del v
buf = bytearray(3)
b.seek(0)
print(b.readinto(buf), buf, b.readable(), b.writable(), b.seekable(), b.isatty())
b.close()
print(b.closed)
for op in (b.read, b.getvalue, b.tell):
    try:
        op()
    except ValueError as e:
        print(e)
try:
    io.BytesIO().write("text")
except TypeError as e:
    print(e)
try:
    io.BytesIO().seek(-1)
except ValueError as e:
    print(e)
print(list(io.BytesIO(b"a\nb\nc")))
w = io.BytesIO()
w.writelines([b"x", bytearray(b"y"), memoryview(b"z")])
print(w.getvalue(), w.__getstate__()[:2])

s = io.StringIO("café\nbar")
print(s.read(2), s.tell(), repr(s.readline()), repr(s.read()))
print(s.seek(0), s.readlines(), s.write("€"), repr(s.getvalue()))
print(s.seek(0, 2), s.tell(), s.truncate(3), repr(s.getvalue()))
print(s.encoding, s.errors, s.line_buffering, s.newlines)
try:
    s.seek(1, 1)
except OSError as e:
    print(e)
try:
    s.write(b"x")
except TypeError as e:
    print(e)
s.close()
try:
    s.read()
except ValueError as e:
    print(e)

for nl in (None, "", "\n", "\r", "\r\n"):
    t = io.StringIO("a\rb\r\nc\nd", newline=nl)
    print(repr(nl), repr(t.getvalue()), t.readlines(), t.newlines)
t = io.StringIO(newline=None)
t.write("x\r\ny\rz\n")
print(repr(t.getvalue()), t.newlines)
try:
    io.StringIO(newline="x")
except ValueError as e:
    print(e)
try:
    io.StringIO(5)
except TypeError as e:
    print(e)

t = io.StringIO()
print(t.write("abc"), t.seek(6), t.write("z"), repr(t.getvalue()))
print(list(io.StringIO("1\n2\n3")))

d = io.IncrementalNewlineDecoder(None, translate=True)
print(repr(d.decode("a\r")), repr(d.decode("\nb\r")), repr(d.decode("", final=True)))
print(d.newlines, d.getstate())
d.setstate((b"", 1))
print(repr(d.decode("x")))
d.reset()
print(d.newlines)
import codecs
d = io.IncrementalNewlineDecoder(codecs.getincrementaldecoder("utf-8")(), translate=False)
print(repr(d.decode(b"\xc3")), repr(d.decode(b"\xa9\r")), d.getstate())
print(repr(d.decode(b"\n", final=True)), d.newlines)
