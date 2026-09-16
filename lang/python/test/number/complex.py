# complex: the literal, the arithmetic, the repr and what it refuses.
#
# Run by CPython to write the golden and by this interpreter to check it. The
# repr is the fiddly half -- a negative zero real part changes whether there
# are parentheses at all -- and the arithmetic is the other, since a textbook
# division and CPython's differ in the last digit.
#
# One thing is deliberately not here: a product of two extreme magnitudes,
# where `ar*br - ai*bi` very nearly cancels. CPython's answer to that depends
# on whether its own C compiler fused the multiply with the subtract -- the
# same expression written in Python gives the other answer -- so the golden
# would record how the host was built and not what Python means.

INF = 1e308 * 10.0
NAN = INF - INF


def show(what, v):
    print(what + " -> " + repr(v))


def attempt(what, fn):
    try:
        print(what + " -> " + repr(fn()))
    except BaseException as e:
        print(what + " -> !" + type(e).__name__)


# ----------------------------------------------------------------- the repr

for v in [0j, 1j, -1j, 2j, 2.5j, 1e3j, complex(0, 0), complex(1, 0),
          complex(0, 1), complex(1, 2), complex(-1, -2), complex(1.5, -2.5),
          complex(-0.0, 1), complex(1, 0.0), complex(1, -0.0),
          complex(0.0, -0.0), complex(-0.0, -0.0),
          complex(INF, 1), complex(1, INF), complex(-INF, -INF),
          complex(NAN, 0), complex(0, NAN), complex(1e300, 1e-300),
          complex(1e-300, 1e300)]:
    print(repr(v) + " | " + str(v))

# --------------------------------------------------------- the constructor

show("complex()", complex())
show("complex(3)", complex(3))
show("complex(3.5)", complex(3.5))
show("complex(True)", complex(True))
show("complex(1, 2)", complex(1, 2))
show("complex(1, 2j)", complex(1, 2j))
show("complex(1j, 2)", complex(1j, 2))
show("complex(1j, 2j)", complex(1j, 2j))
show("complex(1+2j)", complex(1 + 2j))
show("complex(2**64)", complex(2**64))
for s in ["1+2j", "1-2j", "1j", "-1j", "+1j", "2", "2.5", "(1+2j)", " 1+2j ",
          "j", "-j", "+j", "1+j", "1-j", "1e3j", "1E3J", ".5j", "-0j",
          "(1+2j) ", "1 + 2j", "", "abc", "1+2", "2j+1", "1j2"]:
    attempt("complex(" + repr(s) + ")", lambda s=s: complex(s))
attempt("complex('1', 2)", lambda: complex("1", 2))
attempt("complex([])", lambda: complex([]))

# ----------------------------------------------------------- the arithmetic

pairs = [(1 + 2j, 3 + 4j), (1 + 2j, 1), (1, 1 + 2j), (1 + 2j, 2.5), (2.5, 1 + 2j),
         (1 + 2j, 1 + 2j), (0j, 1 + 2j), (1 + 2j, 1j), (3 + 4j, 2**64),
         (1.5 - 0.25j, -3 + 0.125j), (1e-8 + 1e-8j, 1e8 + 2e8j)]
for a, b in pairs:
    show(repr(a) + " + " + repr(b), a + b)
    show(repr(a) + " - " + repr(b), a - b)
    show(repr(a) + " * " + repr(b), a * b)
    show(repr(a) + " / " + repr(b), a / b)
attempt("1j / 0", lambda: 1j / 0)
attempt("1j / 0j", lambda: 1j / 0j)
attempt("1j // 2", lambda: 1j // 2)
attempt("1j % 2", lambda: 1j % 2)
attempt("divmod(1j, 2)", lambda: divmod(1j, 2))

for z, w in [(1 + 2j, 2), (1 + 2j, 3), (1 + 2j, 0), (1 + 2j, 1), (1 + 2j, -1),
             (1 + 2j, -2), (2 + 0j, 0.5), (-1 + 0j, 0.5), (1j, 2), (1j, 1j),
             (0j, 0), (0j, 2), (1 + 1j, 3)]:
    show(repr(z) + " ** " + repr(w), z**w)

show("-(1+2j)", -(1 + 2j))
show("+(1+2j)", +(1 + 2j))
show("abs(3+4j)", abs(3 + 4j))
show("abs(0j)", abs(0j))
show("abs(-3-4j)", abs(-3 - 4j))

# -------------------------------------------------------------- the members

z = 1.5 + 2.5j
show("real", z.real)
show("imag", z.imag)
show("conjugate", z.conjugate())
show("(0j).real", (0j).real)
show("(0j).imag", (0j).imag)
show("(-1j).conjugate()", (-1j).conjugate())

# ------------------------------------------------------- equality and order

show("1+2j == complex(1,2)", 1 + 2j == complex(1, 2))
show("1+0j == 1", 1 + 0j == 1)
show("1+0j == 1.0", 1 + 0j == 1.0)
show("2j == 2", 2j == 2)
show("1+2j != 1+3j", 1 + 2j != 1 + 3j)
show("1+0j == True", 1 + 0j == True)
show("(2**64+0j) == 2**64", (2**64 + 0j) == 2**64)
show("1+2j == 'x'", 1 + 2j == "x")
attempt("1j < 2j", lambda: 1j < 2j)
attempt("1j <= 2j", lambda: 1j <= 2j)
attempt("1j > 2j", lambda: 1j > 2j)
attempt("1j >= 2j", lambda: 1j >= 2j)
attempt("1j < 2", lambda: 1j < 2)
attempt("2 < 1j", lambda: 2 < 1j)

show("hash(1+0j) == hash(1)", hash(1 + 0j) == hash(1))
show("hash(1.5+0j) == hash(1.5)", hash(1.5 + 0j) == hash(1.5))
show("bool(0j)", bool(0j))
show("bool(1j)", bool(1j))
show("a dict", {1 + 2j: "v"}[1 + 2j])
show("a set", (1 + 2j) in {1 + 2j, 3})

# ------------------------------------------------------ what it will not do

attempt("int(1+2j)", lambda: int(1 + 2j))
attempt("float(1+2j)", lambda: float(1 + 2j))
attempt("round(1+2j)", lambda: round(1 + 2j))
attempt("sorted([1j, 2j])", lambda: sorted([1j, 2j]))

# ------------------------------------------------------------ the showing

show("type", type(1j).__name__)
show("isinstance", isinstance(1j, complex))
show("str.format", "{}".format(1 + 2j))
show("percent", "%s" % (1 + 2j))
show("f-string", f"{1 + 2j}")
show("in a list", str([1 + 2j, 0j]))
show("format empty", format(1 + 2j, ""))
