# _struct, array and the memoryview that array finally gives a width to.
import _struct as st
from array import array

print(st.calcsize("<bI"), st.calcsize("@bI"), st.calcsize(">4s2i"), st.calcsize(""))
print(st.pack("<ihb", 1, 2, 3), st.unpack("<ihb", st.pack("<ihb", 1, 2, 3)))
print(st.pack(">I", 1), st.unpack(">I", b"\x00\x00\x01\x00"))
print(st.pack("<d", 1.5), st.unpack("<d", st.pack("<d", 1.5)))
print(st.pack("<f", 1.5), st.unpack("<f", st.pack("<f", 1.5)))
print(st.pack("4s", b"ab"), st.unpack("4s", b"ab\x00\x00"))
print(st.pack("<?h", True, -2), st.unpack("<?h", st.pack("<?h", True, -2)))
print(st.pack("<Q", 2 ** 63), st.unpack("<Q", st.pack("<Q", 2 ** 63)))
print(st.pack("<q", -2 ** 63), st.unpack("<q", st.pack("<q", -2 ** 63)))
print(st.pack("<c", b"z"), st.unpack("<c", b"z"))
print(st.pack("<5p", b"abc"), st.unpack("<5p", st.pack("<5p", b"abc")))
print(st.pack("<2i3x", 1, 2), st.calcsize("<2i3x"))
print(st.pack(">hhh", 1, 2, 3), st.pack("!hhh", 1, 2, 3), st.pack("=hhh", 1, 2, 3))
print(st.unpack_from("<h", b"\x00\x01\x02\x03", 2), st.unpack_from("<h", b"\x01\x00"))
print(list(st.iter_unpack("<h", b"\x01\x00\x02\x00")))

s = st.Struct("<ih")
print(s.size, s.format, s.pack(7, 8), s.unpack(s.pack(7, 8)))
print(s.unpack_from(b"\x00" + s.pack(7, 8), 1), list(s.iter_unpack(s.pack(1, 2))))
ba = bytearray(8)
st.pack_into("<i", ba, 2, 259)
print(ba)
s.pack_into(ba, 0, 1, 2)
print(ba)

for call in ("st.pack('<b', 300)", "st.unpack('<i', b'ab')", "st.calcsize('z')",
             "st.pack('<i')", "st.pack('<Q', -1)", "st.unpack_from('<i', b'ab', 0)"):
    try:
        eval(call)
        print(call, "no error")
    except Exception as e:
        print(call, type(e).__name__ == "error" or type(e).__name__)

a = array("i", [1, 2, 3])
print(a, len(a), a[0], a[-1], list(a), a.typecode, a.itemsize, bool(a), bool(array("i")))
a.append(4)
a.extend([5, 6])
print(a, a[1:4], a[::2], 3 in a, a.count(3), a.index(4))
a[0] = 10
print(a, a.pop(), a.pop(0), a)
a.insert(0, 99)
a.reverse()
print(a, a.tolist(), a.tobytes())
b = array("i")
b.frombytes(a.tobytes())
print(b, b == a, b == array("i", [1]), array("i", [1]) < array("i", [2]))
b.remove(99)
print(b, array("i", b))
print(array("h", b"22"), array("i", bytearray(4)), array("H", array("b", [1, 2])))
u = array("u", "héllo")
print(u, u.tounicode(), len(u), u.itemsize)
u.fromunicode("!")
print(u)
d = array("d", [1.5, 2.5])
print(d, d.tolist(), d.itemsize, array("f", [0.5]))
print(array("b", [1]) + array("b", [2]), array("b", [1]) * 3)
sw = array("h", [1, 2])
sw.byteswap()
print(sw, sw.buffer_info()[1])
print(array("b", [1, 2, 3]).tolist(), list(reversed(array("b", [1, 2]))))

for call in ("array('z')", "array('i', [1.5])", "array('b', [300])",
             "array('i').pop()", "array('i', [1]).remove(2)",
             "array('i', [1]).extend(array('b', [1]))"):
    try:
        eval(call)
        print(call, "no error")
    except Exception as e:
        print(call, type(e).__name__)

m = memoryview(a)
print(m.itemsize, m.format, m.shape, m.strides, m.nbytes, m.ndim, m.readonly, len(m))
print(m[0], m.tolist(), m.c_contiguous, m.obj is a)
m2 = memoryview(bytearray(b"abcd"))
print(m2.itemsize, m2.format, m2.tolist(), m2[1], bytes(m2), m2.nbytes)
m2[0] = 65
print(bytes(m2), m2[1:3].tolist(), m2[::2].tolist(), m2[::-1].tolist())
print(m2[::2].strides, m2[::2].c_contiguous, m2[::2].tobytes())
c = m2.cast("H")
print(c.itemsize, c.format, c.tolist(), len(c), c.nbytes)
mb = memoryview(b"xy")
print(mb.readonly, mb.tolist(), mb.tobytes(), mb.hex())
md = memoryview(d)
print(md.format, md.itemsize, md.tolist(), md.shape)
md[0] = 9.5
print(d, md.tolist())
