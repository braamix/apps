# marshal: every version of the format, references, and the errors. Nothing
# here is shared except what CPython marks for reuse whatever its count.
import io
import marshal
import sys

vals = [None, True, False, Ellipsis, StopIteration, 0, 1, -1, 2**31 - 1, -2**31, 2**31,
        -2**31 - 1, 2**64, -2**100, 10**50, 1.5, -0.0, float("inf"), 1e300, 3j, complex(1.5, -2),
        b"", b"abc", bytearray(b"xy"), memoryview(b"mv"), "", "abc", "é", "a\udc80b",
        "日本", (), (1, 2), tuple(range(300)), {"a": 1, 2: (3,)}, {1, 2, 3},
        slice(1, None, 3), sys.intern("hello"), ("interned_name", "interned_name"), frozenset()]
for v in vals:
    for ver in range(0, 6):
        try:
            d = marshal.dumps(v, ver)
        except Exception as e:
            print(ver, type(e).__name__, e)
            continue
        back = marshal.loads(d)
        print(ver, d.hex()[:80], repr(back)[:50], back == v or repr(back) == repr(v))
fs = frozenset({"x", "y", 1})
print([marshal.dumps(fs, v).hex() for v in (2, 3, 4)], marshal.loads(marshal.dumps(fs)) == fs)

for bad in [b"", b"i\x01", b"r\x00\x00\x00\x00", b"q", b"(\x01\x00\x00\x00", b"0",
            b"[\x01\x00\x00\x000", b"l\x01\x00\x00\x00\x00\x00", b"{i\x01\x00\x00\x00",
            b"c", bytes([0x80 | ord("(")]) + b"\x01\x00\x00\x00r\x00\x00\x00\x00"]:
    try:
        print(repr(marshal.loads(bad)))
    except Exception as e:
        print(type(e).__name__, e)
x = marshal.loads(b"\xdb\x01\x00\x00\x00r\x00\x00\x00\x00")
print(x[0] is x)
l = [1]
l.append(l)
x = marshal.loads(marshal.dumps(l))
print(x[1] is x, len(x))
t = ([],)
t[0].append(t)
for bad in (object(), t):
    try:
        marshal.dumps(bad)
    except ValueError as e:
        print(e)
try:
    marshal.dumps(compile("1", "", "eval"), allow_code=False)
except ValueError as e:
    print(e)
try:
    marshal.loads(b"c", allow_code=False)
except ValueError as e:
    print(e)
deep = []
for i in range(3000):
    deep = [deep]
try:
    marshal.dumps(deep)
except ValueError as e:
    print(e)
d = b"[\x01\x00\x00\x00" * 1999 + b"N"
print(len(marshal.loads(d)))
try:
    marshal.loads(b"[\x01\x00\x00\x00" * 2000 + b"N")
except ValueError as e:
    print(e)
print(marshal.version, marshal.dumps({1: 2}, 6) == marshal.dumps({1: 2}))
print(marshal.loads(marshal.dumps(frozendict(a=1, b=(2,)))))
try:
    marshal.dumps(frozendict(a=1), 5)
except ValueError as e:
    print(e)

f = io.BytesIO()
marshal.dump([1, "a"], f)
marshal.dump(2, f)
f.seek(0)
print(marshal.load(f), marshal.load(f))
try:
    marshal.load(f)
except EOFError as e:
    print(e)
big = [{"k%d" % i: (i, float(i), {i, -i}, b"x" * (i % 50))} for i in range(500)]
print(marshal.loads(marshal.dumps(big)) == big)
print(marshal.loads(marshal.dumps(-(2**1000) + 12345)) == -(2**1000) + 12345)
for n in (2**15, 2**30, 2**45, -2**45 + 1, 2**62 - 1):
    print(marshal.dumps(n).hex(), marshal.loads(marshal.dumps(n)) == n)
print(marshal.loads(b"I\x01\x02\x03\x04\x05\x06\x07\x08"))
print(marshal.loads(b"\x7b\x69\x01\x00\x00\x00\x69\x02\x00\x00\x00\x30trailing"))
for bad in [(1, "x"), (1, 2.5)]:
    try:
        marshal.dumps(*bad)
    except TypeError as e:
        print(e)
for bad in ["str", 5]:
    try:
        marshal.loads(bad)
    except TypeError as e:
        print(e)
print(marshal.loads(bytearray(b"i\x05\x00\x00\x00")), marshal.loads(memoryview(b"N")))
