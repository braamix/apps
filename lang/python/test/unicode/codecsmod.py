# codecs.py and the encodings package, as CPython ships them, over the native
# _codecs: every codec written natively, every built-in error handler, one of
# the program's own, and the registry.
#
# The same program on CPython and here, byte for byte.

import codecs

TEXT = "a\xe9\u20ac\U0001f600z"
for enc in ["utf-8", "utf-16", "utf-16-le", "utf-16-be", "utf-32", "utf-32-le",
            "utf-32-be", "utf-7", "unicode_escape", "raw_unicode_escape", "utf-8-sig"]:
    b = TEXT.encode(enc)
    print(enc, b, ascii(b.decode(enc)), codecs.lookup(enc).name)
for enc in ["latin-1", "iso8859-15", "cp1252", "cp437", "koi8-r", "mac-roman", "ascii"]:
    for h in ["replace", "ignore", "backslashreplace", "xmlcharrefreplace", "namereplace"]:
        print(enc, h, "a\xe9\u20ac\u0444z".encode(enc, h))
    print(enc, ascii(bytes(range(0x80, 0x100)).decode(enc, "replace")))

# Decoding errors, with each handler, in each native codec.
BAD = [("utf-8", b"a\x80b\xe2\x82c\xf0\x9f\x98\xed\xa0\x80d\xc0\xafe\xf4\x90\x80\x80"),
       ("utf-8", b"\xe2\x82"), ("utf-8", b"\xed\xa0"), ("ascii", b"a\xffb\x80"),
       ("utf-16-le", b"a\x00\x00\xd8b\x00\x00\xdc"), ("utf-16-be", b"\xd8\x00\xd8\x00"),
       ("utf-16", b"\xff\xfea"), ("utf-32-le", b"\x00\xd8\x00\x00a\x00\x00\x00\x00\x00\x11\x00"),
       ("utf-32-be", b"\x00\x00\x00a\x00"), ("utf-7", b"a+2D3-b+A-c+AB-d\x80"),
       ("utf-7", b"+2D3"), ("unicode_escape", b"a\\x4g\\N{NOPE}\\U00110000\\"),
       ("raw_unicode_escape", b"\\u12x\\U00110000"), ("cp1252", b"a\x81\x8db")]
for enc, data in BAD:
    for h in ["strict", "replace", "ignore", "backslashreplace", "surrogateescape",
              "surrogatepass"]:
        try:
            print(enc, h, ascii(data.decode(enc, h)))
        except UnicodeDecodeError as e:
            print(enc, h, e, e.start, e.end, e.object == data, e.encoding)

# Encoding errors: surrogates, and what latin-1 and ascii cannot say.
for enc, text in [("utf-8", "a\ud800\udfffb\udc80"), ("utf-16-le", "a\ud800b"),
                  ("utf-32", "\udc80"), ("latin-1", "a\u0100\u0101b\udcff"),
                  ("ascii", "a\xe9\udc80\ud800"), ("cp1252", "a\u0101\udc80")]:
    for h in ["strict", "replace", "ignore", "backslashreplace", "xmlcharrefreplace",
              "namereplace", "surrogateescape", "surrogatepass"]:
        try:
            print(enc, h, text.encode(enc, h))
        except UnicodeEncodeError as e:
            print(enc, h, e, e.start, e.end, ascii(e.object), e.reason)

# A handler of the program's own: for either direction, with str or bytes.
def mine(exc):
    print("  handler", type(exc).__name__, exc.encoding, exc.start, exc.end, exc.reason)
    if isinstance(exc, UnicodeDecodeError):
        return ("<%s>" % exc.object[exc.start:exc.end].hex(), exc.end)
    return ("<%d>" % exc.start, exc.end) if exc.start % 2 else (b"!!", exc.end)
codecs.register_error("test.mine", mine)
print("x\xe9y\u20acz\u20ac".encode("ascii", "test.mine"))
print("x\ud800y".encode("utf-16-be", "test.mine"))
print(ascii(b"x\xffy\xe2\x82".decode("utf-8", "test.mine")))
print(ascii(b"a\x81b".decode("cp1252", "test.mine")))
print(codecs.lookup_error("test.mine") is mine)

calls = []
def back(exc):
    calls.append(exc.start)
    return ("<", -3) if len(calls) == 1 else (">", exc.end)
codecs.register_error("test.back", back)
print("ab\xe9".encode("ascii", "test.back"))
for ret in [("x",), "x", ("x", "y"), (5, 1), ("x", 100)]:
    codecs.register_error("test.bad", lambda exc, ret=ret: ret)
    try:
        "a\xe9".encode("ascii", "test.bad")
    except (TypeError, IndexError) as e:
        print(type(e).__name__, e)
codecs.register_error("test.nonascii", lambda exc: ("\xe9", exc.end))
try:
    "a\u20ac".encode("ascii", "test.nonascii")
except UnicodeEncodeError as e:
    print(e)
print("a\u20ac".encode("latin-1", "test.nonascii"))

# The handlers themselves, called by hand.
for name in ["strict", "ignore", "replace", "xmlcharrefreplace", "backslashreplace",
             "namereplace", "surrogateescape", "surrogatepass"]:
    h = codecs.lookup_error(name)
    for exc in [UnicodeEncodeError("utf-8", "a\ud800\udc80\xe9", 1, 3, "r"),
                UnicodeDecodeError("utf-16-le", b"a\x00\x00\xd8", 2, 4, "r"),
                UnicodeTranslateError("a\u20acb", 1, 2, "r"),
                UnicodeEncodeError("ascii", "abc", 5, 1, "r")]:
        try:
            print(name, ascii(h(exc)))
        except Exception as e:
            print(name, type(e).__name__, e)
try:
    codecs.lookup_error("test.nope")
except LookupError as e:
    print(e)
try:
    codecs.register_error("test.x", 5)
except TypeError as e:
    print(e)

# The exceptions: what their constructors check, and how they read.
for args in [("utf-8", "abc", 1, 2, "r"), ("utf-8", "abc", 0, 3, "r"),
             ("utf-8", "abc", 5, 7, "r"), ("utf-8", "\U0001f600", 0, 1, "r")]:
    e = UnicodeEncodeError(*args)
    print(repr(e), str(e), e.args == args)
e = UnicodeDecodeError("x", bytearray(b"ab"), 1, 2, "why")
print(repr(e), e.object, str(e))
e.start, e.end, e.reason = 0, 2, "because"
print(str(e), e.start, e.end, e.reason)
print(str(UnicodeTranslateError("\u20ac", 0, 1, "no")), UnicodeTranslateError("ab", 0, 2, "x"))
for cls, args in [(UnicodeEncodeError, ()), (UnicodeEncodeError, ("a", b"b", 0, 1, "r")),
                  (UnicodeDecodeError, ("a", "b", 0, 1, "r")),
                  (UnicodeEncodeError, ("a", "b", "0", 1, "r")),
                  (UnicodeTranslateError, ("a", 0, 1))]:
    try:
        cls(*args)
    except TypeError as e:
        print(cls.__name__, e)
class Mine(UnicodeEncodeError):
    pass
print(str(Mine("m", "xyz", 1, 2, "own")))

# The registry: search functions, the cache, lookup, and the text-encoding check.
def search(name):
    if name == "test_rot":
        return codecs.CodecInfo(lambda s, errors="strict": (s[::-1].encode(), len(s)),
                                lambda b, errors="strict": (bytes(b)[::-1].decode(), len(b)),
                                name="test-rot")
    if name == "test_bytes":
        return codecs.CodecInfo(lambda s, errors="strict": (s, len(s)),
                                lambda b, errors="strict": (b, len(b)), name="test-bytes",
                                _is_text_encoding=False)
    return None
codecs.register(search)
print("abc".encode("Test Rot"), b"cba".decode("test-rot"), codecs.lookup("TEST-ROT").name)
for f in [lambda: "x".encode("test-bytes"), lambda: b"x".decode("test-bytes"),
          lambda: "x".encode("test-nope"), lambda: codecs.lookup("test-nope")]:
    try:
        f()
    except LookupError as e:
        print(e)
print(codecs.encode(b"x", "test-bytes"), codecs.decode(b"y", "test-bytes"))
codecs.unregister(search)
try:
    "x".encode("test-rot")
except LookupError as e:
    print(e)
try:
    "\u0101".encode("cp1252")
except UnicodeEncodeError as e:
    print(e, e.__notes__)
try:
    codecs.decode(b"\xff", "utf-8")
except UnicodeDecodeError as e:
    print(e, e.__notes__)
print(codecs.encode("abc", "rot13"), codecs.decode("nop", "rot_13"))

# The incremental codecs, fed a byte at a time.
for enc, data in [("utf-8", "a\u20ac\U0001f600".encode()), ("utf-16", "a\u20ac".encode("utf-16")),
                  ("utf-32-be", "a\U0001f600".encode("utf-32-be")),
                  ("utf-7", "a\u20acb\u20ac".encode("utf-7")),
                  ("unicode_escape", b"a\\u20ac\\N{EM DASH}b"), ("utf-8-sig", b"\xef\xbb\xbfx")]:
    d = codecs.getincrementaldecoder(enc)()
    out = [d.decode(data[i:i + 1]) for i in range(len(data))]
    print(enc, ascii(out), ascii(d.decode(b"", final=True)))
    e = codecs.getincrementalencoder(enc)()
    print(enc, [e.encode(c) for c in "ab"], e.encode("", final=True))
try:
    codecs.getincrementaldecoder("utf-8")().decode(b"\xe2\x82", final=True)
except UnicodeDecodeError as e:
    print(e)

# The functions _codecs has for each codec.
print(codecs.utf_8_decode(b"a\xe2\x82"), codecs.utf_8_decode(b"a\xe2\x82", "strict", True)
      if False else None, codecs.utf_16_ex_decode(b"\xfe\xff\x00a", None, 0, False))
print(codecs.utf_16_ex_decode(b"a\x00", None, 0, True), codecs.utf_32_ex_decode(b"", None, 0, True))
print(codecs.utf_16_encode("a", None, 1), codecs.utf_32_encode("a", None, -1))
print(codecs.escape_decode(b"a\\x41\\n\\'\\101"), codecs.escape_encode(b"a'\\\x00\xff"))
for how in ["strict", "replace", "ignore", "other"]:
    try:
        print(codecs.escape_decode(b"x\\x4", how))
    except ValueError as e:
        print(e)
print(codecs.charmap_decode(b"\x00\x01\x02", "replace", "ab"),
      codecs.charmap_decode(b"\x00\x01", "strict", {0: "x", 1: 0x263a}))
m = codecs.charmap_build("\x00ab\ufffec")
print(type(m).__name__, codecs.charmap_encode("abc", "replace", m) if False else
      codecs.charmap_encode("acb", "strict", m), codecs.charmap_encode("ab", "strict", {97: b"XY", 98: 7}))
try:
    codecs.charmap_encode("ab", "strict", {97: 300})
except TypeError as e:
    print(e)
print(codecs.readbuffer_encode(b"ab"), codecs.readbuffer_encode("\xe9"))
print(codecs.latin_1_decode(b"\xe9"), codecs.ascii_encode("ab"), codecs.latin_1_encode("\xe9"))
print(codecs.unicode_escape_decode(b"a\\u20", None, False), codecs.raw_unicode_escape_decode("\\u20ac"))

# str(), bytes() and bytearray() with an encoding, positionally and by name.
print(str(b"caf\xe9", "latin-1"), str(b"a\xffb", errors="replace"), str(object=b"x"),
      bytes("\xe9", "latin-1"), bytes("\xe9", encoding="utf-16-be"),
      bytearray("\u20ac", "cp1252"), bytearray(source="x", encoding="ascii"))
for f in [lambda: str("x", "utf-8"), lambda: bytes("x"), lambda: bytes(5, "utf-8"),
          lambda: bytes(b"x", errors="strict"), lambda: str(b"x", 5),
          lambda: "x".encode(5), lambda: b"x".decode("utf-8", 5)]:
    try:
        f()
    except TypeError as e:
        print(e)
