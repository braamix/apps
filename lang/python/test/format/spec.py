# The format-spec mini-language, through format() alone: fill, align, sign,
# '#', '0', width, grouping, precision and type, over str, int, bool and float.
#
# Run by CPython to write the golden and by this interpreter to check it, so
# nothing here may need what the interpreter has not got: no integer past
# 2**30, no f-string (fstring.py has those), no generator.

INF = 1e308 * 10.0
NAN = INF - INF


def show(v, spec):
    try:
        got = format(v, spec)
    except BaseException as e:
        got = "!" + type(e).__name__
    print(repr(v) + " : " + repr(spec) + " -> " + repr(got))


# ------------------------------------------------------------------- str

for spec in ["", "s", "5", "<5", ">5", "^5", "=5", "5s", ".2", ".0", ".10",
             "*<6", "*>6", "*^6", "-^7", "8.2", "0", "05"]:
    show("ab", spec)
for spec in ["", "6", "<6", "^6", ".2"]:
    show("", spec)
    show("héllo", spec)

# ------------------------------------------------------------------- int

for spec in ["", "d", "5", "<5", ">5", "^5", "=5", "05", "+", "-", " ",
             "+d", " d", "+05", "-05", " 05", "b", "o", "x", "X", "c",
             "#b", "#o", "#x", "#X", "#010b", "#010x", "#010X",
             ",", "_", ",d", "_d", "012,d", "012_d", "020,d",
             "e", "E", "f", "F", "g", "G", "%", ".2f", ".3e", ".0f",
             "n", ",n", "*^9d", "=+9d", "s"]:
    show(7, spec)
    show(-7, spec)
for spec in ["", "d", ",d", "012,d", "015,d", "_d", "x", "#x", "020,d", "06,d"]:
    show(1234567, spec)
    show(-1234567, spec)
show(0, "05,d")
show(0, "#o")
show(0, "#x")
show(0, "#b")
show(65, "c")
show(0x2603, "c")
print(ord(format(0x10FFFF, "c")))
show(-1, "c")
show(255, "08b")

# ------------------------------------------------------------------ bool

for spec in ["", "d", "5", "s", "x", "f", "05d", ",d"]:
    show(True, spec)
    show(False, spec)

# ----------------------------------------------------------------- float

for spec in ["", "f", "F", "e", "E", "g", "G", "%", "n",
             ".0f", ".1f", ".2f", ".6f", ".0e", ".3e", ".0g", ".3g", ".10g",
             "10.2f", "<10.2f", ">10.2f", "^10.2f", "=10.2f", "010.2f",
             "+.2f", "-.2f", " .2f", "+010.2f", ",.2f", "_.2f", "020,.4f",
             "8.0f", ".1%", "*^12.3f", "d"]:
    show(3.14159, spec)
    show(-3.14159, spec)
for v in [0.0, -0.0, 1.0, 0.5, 2.5, 1.5, 0.125, 1.005, 100000.0, 1000000.0,
          0.0001, 0.00001, 1e16, 1e20, 1e-10, 123456789.0, 1e308]:
    for spec in ["", "f", "e", "g", ".1f", ".3g", ",.2f"]:
        show(v, spec)
for v in [INF, -INF, NAN]:
    for spec in ["", "f", "F", "e", "E", "g", "G", "10f", "<10f", "^10f",
                 "010f", "+f", " f"]:
        show(v, spec)

# ------------------------------------------------------- what is refused

for spec in ["q", "5.2q", ".", "5.", "zz", "<<<5", "99999999999999999999d"]:
    show(1, spec)
    show("a", spec)
    show(1.0, spec)

# ------------------------------------------------ the default __format__

print(format(None, ""))
print(format([1, 2], ""))
show(None, "5")
show([1], "5")


class Plain:
    def __repr__(self):
        return "<plain>"


class Own:
    def __format__(self, spec):
        return "OWN(" + repr(spec) + ")"


class ViaStr:
    def __str__(self):
        return "via-str"

    def __repr__(self):
        return "<via-repr>"


print(format(Plain(), ""))
print(format(Own(), ""))
print(format(Own(), ">10.3f"))
print(format(ViaStr(), ""))
show(Plain(), "5")
show(ViaStr(), "5")
