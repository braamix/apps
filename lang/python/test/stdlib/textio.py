# TextIOWrapper: tell and seek cookies, codecs native and not, reconfigure.
import codecs
import io

def wrap(data=b"", **kw):
    return io.TextIOWrapper(io.BytesIO(data), **kw)

t = wrap("αβγ\nδε\nlast".encode(), encoding="utf-8")
print(t.readline(), t.tell())
c = t.tell()
print(repr(t.read(1)), t.tell(), repr(t.readline()))
t.seek(c)
print(repr(t.read()), t.tell())
t.seek(0)
print(t.readlines(), t.seek(0, 2))
for bad in ((1, 1), (-1, 0), (0, 3)):
    try:
        t.seek(*bad)
    except (OSError, ValueError) as e:
        print(type(e).__name__, e)

for enc in ("utf-16", "utf-16-le", "utf-32", "cp1252", "koi8-r", "utf-8-sig", "cp437"):
    text = {"koi8-r": "при\nвет\n", "cp437": "Ça ░\nfin\n"}.get(enc, "Straße €\nzwei\n")
    b = io.BytesIO()
    w = io.TextIOWrapper(b, encoding=enc)
    w.write(text)
    w.flush()
    raw = b.getvalue()
    b.seek(0)
    r = io.TextIOWrapper(b, encoding=enc)
    first = r.readline()
    pos = r.tell()
    rest = r.read()
    r.seek(pos)
    print(enc, len(raw), repr(first), rest == r.read(), r.read() == "")
    w.detach()

# A decoder with state across a seek: every position read back.
data = "aé中\U0001f600b\r\nc".encode("utf-16")
t = wrap(data, encoding="utf-16", newline="")
cookies = []
while True:
    cookies.append(t.tell())
    if not t.read(1):
        break
got = []
for c in cookies:
    t.seek(c)
    got.append(t.read(1))
print(got)

t = wrap(b"one\r\ntwo\rthree\n", encoding="ascii")
print(t.readlines(), t.newlines)
t = wrap(b"one\r\ntwo\rthree\n", encoding="ascii", newline="\r")
print(t.readlines())
t = wrap(b"x\r", encoding="ascii", newline=None)
print(repr(t.read()))

b = io.BytesIO()
t = io.TextIOWrapper(b, encoding="ascii", errors="backslashreplace", newline="\r\n")
t.write("é\nline\n")
t.flush()
print(b.getvalue(), t.errors, t.encoding, t.newlines, t.line_buffering, t.write_through)
t.reconfigure(errors="replace", line_buffering=True)
t.write("ü\n")
print(b.getvalue(), t.errors, t.line_buffering)
t.reconfigure(encoding="utf-8", newline="\n")
t.write("ü\n")
t.flush()
print(b.getvalue())
try:
    t.reconfigure(newline="x")
except ValueError as e:
    print(e)
t.seek(0)
print(repr(t.read()))
try:
    t.reconfigure(encoding="latin-1")
except io.UnsupportedOperation as e:
    print(type(e).__name__, e)

t = wrap(b"\xff abc", encoding="utf-8")
try:
    t.read()
except UnicodeDecodeError as e:
    print(e.reason, e.start, e.end)
t = wrap(b"", encoding="ascii")
try:
    t.write("é")
    t.flush()
except UnicodeEncodeError as e:
    print(e.reason)
t = wrap(b"data", encoding="latin-1")
print(t.buffer.__class__.__name__, t.name if hasattr(t, "name") else "noname", repr(t))
print(t.detach().getvalue())
try:
    t.read()
except ValueError as e:
    print(e)
try:
    io.TextIOWrapper(io.BytesIO(), encoding="no-such-codec")
except LookupError as e:
    print(e)
try:
    io.TextIOWrapper(io.BytesIO(), encoding="rot13")
except LookupError as e:
    print(e)
print(io.text_encoding(None), io.text_encoding("x"), codecs.lookup("utf8").name)

# Chunked reads across a multibyte boundary.
t = wrap(("é" * 5000).encode(), encoding="utf-8")
t._CHUNK_SIZE = 3
print(len(t.read(4999)), t._CHUNK_SIZE, repr(t.read()), t.tell())
