# str.format and str.format_map: the replacement-field syntax, which is the
# third grammar over the one spec engine.
#
# Run by CPython to write the golden and by this interpreter to check it.


def show(fmt, *args, **kwargs):
    try:
        got = fmt.format(*args, **kwargs)
    except BaseException as e:
        got = "!" + type(e).__name__
    print(repr(fmt) + " -> " + repr(got))


def showmap(fmt, m):
    try:
        got = fmt.format_map(m)
    except BaseException as e:
        got = "!" + type(e).__name__
    print(repr(fmt) + " map -> " + repr(got))


# --------------------------------------------------------------- the numbering

show("{}", 1)
show("{} {}", 1, 2)
show("{0} {1} {0}", "a", "b")
show("{1}{0}", "a", "b")
show("{} {0}", 1)
show("{0} {}", 1)
show("{}", )
show("{5}", 1)
show("{a}{b}", a=1, b=2)
show("{a}", 1)
show("{0}{a}", 9, a=1)
show("no fields")
show("{{}}")
show("{{{}}}", 1)
show("}}{{")
show("{", 1)
show("}", 1)
show("{0", 1)

# ------------------------------------------------------------------ the trail

show("{0[0]}", [10, 20])
show("{0[1]}", [10, 20])
show("{0[k]}", {"k": 9})
show("{0[k][1]}", {"k": [7, 8]})
show("{0[9]}", [1])
show("{0[x]}", {})


class Node:
    def __init__(self, v):
        self.v = v
        self.kid = None

    def __repr__(self):
        return "Node(" + repr(self.v) + ")"


n = Node(1)
n.kid = Node(2)
show("{0.v}", n)
show("{0.kid.v}", n)
show("{0.nope}", n)
show("{n.v}", n=n)

# ---------------------------------------------------------------- the spec

show("{:>10}", "hi")
show("{:<10}|", "hi")
show("{:^10}|", "hi")
show("{:*^10}", "hi")
show("{:10.2}", "abcdef")
show("{:d}", 42)
show("{:5d}", 42)
show("{:05d}", -42)
show("{:,}", 1234567)
show("{:_x}", 0xBC614E)
show("{:.3f}", 3.14159)
show("{:+.2e}", 31415.9)
show("{:%}", 0.25)
show("{:c}", 65)
show("{:s}", 5)
show("{:d}", "x")

# ------------------------------------------------------------ a nested spec

show("{:{}}", 7, ">5")
show("{:{}{}}", 7, ">", 5)
show("{:>{w}}", 7, w=6)
show("{:{a}.{b}f}", 3.14159, a=10, b=3)
show("{0:{1}}", "x", "^7")
show("{:{}}", 7)
show("{:{:{}}}", 7, 5, 3)

# ---------------------------------------------------------- the conversions

show("{!r}", "x")
show("{!s}", "x")
show("{!a}", "héllo")
show("{!r:>8}", "x")
show("{!q}", "x")
show("{0!r} {0!s}", n)

# ------------------------------------------------------------- format_map


class Defaults:
    def __getitem__(self, key):
        return "<" + key + ">"


showmap("{a} {b}", {"a": 1, "b": 2})
showmap("{a}", {})
showmap("{anything}", Defaults())
showmap("{x!r:>6}", Defaults())

# ------------------------------------------------------------- the classes


class Own:
    def __format__(self, spec):
        return "OWN(" + spec + ")"

    def __repr__(self):
        return "own-repr"


class ViaStr:
    def __str__(self):
        return "via-str"

    def __repr__(self):
        return "via-repr"


o = Own()
v = ViaStr()
show("{}", o)
show("{:xyz}", o)
show("{!r}", o)
show("{:{}}", o, "abc")
show("{}", v)
show("{!r}", v)
show("{:>10}", v)
show("{}", [v])
show("{!r}", [v])
show("{} {} {}", o, v, 1)
