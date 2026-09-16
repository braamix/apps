# `%` on str and on bytes: the older grammar, and the one place where the two
# rulers disagree about what a precision means.
#
# Run by CPython to write the golden and by this interpreter to check it.

INF = 1e308 * 10.0


def show(fmt, arg):
    try:
        got = fmt % arg
    except BaseException as e:
        got = "!" + type(e).__name__
    print(repr(fmt) + " % " + repr(arg) + " -> " + repr(got))


# ----------------------------------------------------------------- the types

for f in ["%s", "%r", "%a", "%d", "%i", "%u", "%o", "%x", "%X",
          "%e", "%E", "%f", "%F", "%g", "%G", "%c", "%%", "%q"]:
    show(f, 7)
    show(f, -7)
    show(f, 3.5)
    show(f, "ab")

# ----------------------------------------------------------------- the flags

for f in ["%10s", "%-10s", "%.2s", "%10.2s", "%-10.2s"]:
    show(f, "abcdef")
    show(f, 12345)
for f in ["%5d", "%-5d", "%05d", "%+5d", "% 5d", "%+05d", "%-05d", "% d", "%+d",
          "%.4d", "%.0d", "%8.4d", "%-8.4d", "%08.4d", "%#x", "%#o", "%#X",
          "%#8.4x", "%.4x", "%#.4x"]:
    show(f, 12)
    show(f, -12)
    show(f, 0)
for f in ["%8.2f", "%-8.2f", "%08.2f", "%+.3e", "%.0f", "%10.4g", "%#.0f"]:
    show(f, 3.14159)
    show(f, -3.14159)
    show(f, 0.0)
for f in ["%f", "%e", "%g", "%10f", "%-10f", "%010f"]:
    show(f, INF)
    show(f, -INF)
    show(f, INF - INF)

# ------------------------------------------------------------- the arguments

show("%s %s", ("a", "b"))
show("%s", ("a",))
show("%s %s", "a")
show("%s", ())
show("%d items", 3)
show("%s", [1, 2])
show("%s", {})
show("%s", ({},))
show("foo", {})
show("%(a)s-%(b)d", {"a": "x", "b": 2})
show("%(a)s", {})
show("%(a)s", 1)
show("%s %(a)s %(a)s", {"a": 1})
show("%(a)s %s %(a)s", {"a": 1})
show("%*d", (6, 42))
show("%-*d", (6, 42))
show("%*d", (-6, 42))
show("%.*f", (3, 1.23456))
show("%*.*f", (10, 2, 1.23456))
show("%*d", 5)
show("%", 1)
show("%(", 1)

# --------------------------------------------------------------------- chars

show("%c", 65)
show("%c", "z")
show("%c", "zz")
show("%c", -1)
show("%c", True)
show("%5c", 65)
show("%-5c", 65)

# --------------------------------------------------------------------- bytes

for f in [b"%s", b"%b", b"%d", b"%x", b"%5s", b"%-5s", b"%.2s", b"%f", b"%c"]:
    show(f, b"ab")
    show(f, 7)
show(b"%s %d", (b"ab", 7))
show(b"%%", ())
show(b"%(k)s", {b"k": b"v"} if False else {"k": b"v"})

# --------------------------------------------------------------- the classes


class WithStr:
    def __str__(self):
        return "S"

    def __repr__(self):
        return "R"


class WithInt:
    def __int__(self):
        return 123

    def __repr__(self):
        return "I"


class Nothing:
    def __repr__(self):
        return "N"


show("%s", WithStr())
show("%r", WithStr())
show("%a", WithStr())
show("%d", WithInt())
show("%5d", WithInt())
show("%s", WithInt())
show("%d", Nothing())
show("%s", [WithStr()])
show("%r", [WithStr()])
show("%s", (WithStr(), WithStr()))
