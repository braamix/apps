# Integers past the value word: the arithmetic, the conversions and the repr.
#
# Run by CPython to write the golden and by this interpreter to check it, so
# what is compared is the answer and not a hand-written guess at it. Long
# division and the bitwise operators over infinite two's complement are what
# this is really for: both have many nearly-right answers.

SMALL = 2**30 - 1  # the last value that stays in the word


def show(what, v):
    print(what + " -> " + repr(v))


# ------------------------------------------------------- promotion and back

for a in [SMALL, SMALL + 1, -SMALL, -SMALL - 1, -SMALL - 2, 0, 1, -1]:
    show("int(" + str(a) + ")", a)
    show("type", type(a).__name__)
show("promote", SMALL + 1)
show("demote", (SMALL + 1) - 1)
show("demote-mul", (2**64 // 2**64))
show("zero", 2**64 - 2**64)
show("round-trip", int(str(2**200)) == 2**200)

# ---------------------------------------------------------------- the four

A = 123456789012345678901234567890
B = 98765432109876543210
C = -A
for x in [A, C, B, -B, 0, 1, -1, 2**64, -(2**64)]:
    for y in [B, -B, 1, -1, 7, -7, 2**32, 2**64 + 1]:
        show(str(x) + " + " + str(y), x + y)
        show(str(x) + " - " + str(y), x - y)
        show(str(x) + " * " + str(y), x * y)
        show(str(x) + " // " + str(y), x // y)
        show(str(x) + " % " + str(y), x % y)
        show(str(x) + " divmod " + str(y), divmod(x, y))
        show(str(x) + " == q*y+r", x // y * y + x % y == x)

for y in [0]:
    for op in ["//", "%"]:
        try:
            print(eval_result := (A // y if op == "//" else A % y))
        except ZeroDivisionError:
            show(str(A) + " " + op + " 0", "ZeroDivisionError")

# ------------------------------------------------------------- the bitwise

for x in [A, C, B, -B, 0, -1, 1, 2**64, -(2**64), 2**64 - 1]:
    for y in [B, -B, 0, -1, 1, 255, -256, 2**70]:
        show(str(x) + " & " + str(y), x & y)
        show(str(x) + " | " + str(y), x | y)
        show(str(x) + " ^ " + str(y), x ^ y)
    show("~" + str(x), ~x)
    show("-" + str(x), -x)
    show("abs " + str(x), abs(x))

# --------------------------------------------------------------- the shifts

for x in [A, C, 1, -1, 0, 2**64, -(2**64) - 1]:
    for n in [0, 1, 7, 31, 32, 33, 64, 65, 100, 200]:
        show(str(x) + " << " + str(n), x << n)
        show(str(x) + " >> " + str(n), x >> n)

# ------------------------------------------------------------- the ordering

vals = [0, 1, -1, SMALL, SMALL + 1, -SMALL - 1, B, -B, A, C, 2**64, -(2**64)]
for x in vals:
    for y in vals:
        if (x < y) + (x == y) + (x > y) != 1:
            print("BROKEN trichotomy", x, y)
print("trichotomy holds over", len(vals), "values")
print(sorted(vals))
print(max(vals), min(vals))

# ---------------------------------------------------- against float, exactly

BIG53 = 2**53
for x in [BIG53, BIG53 + 1, BIG53 + 2, 2**64, 2**64 + 1, -(2**64)]:
    f = float(x)
    show(str(x) + " == float", x == f)
    show(str(x) + " < float", x < f)
    show(str(x) + " > float", x > f)
    show("float(" + str(x) + ")", f)
show("int(1e30)", int(1e30))
show("int(-1e30)", int(-1e30))
show("int(2.9)", int(2.9))
show("int(-2.9)", int(-2.9))
show("2**64 == 2.0**64", 2**64 == 2.0**64)
show("2**1024 overflow", float(2**1023) * 2)

# ------------------------------------------------------------ the divisions

for x in [A, B, 2**64, 2**53 + 1, 1, 7]:
    for y in [B, 3, 7, 2**64, 2**53 + 1]:
        show(str(x) + " / " + str(y), x / y)
show("1/3", 1 / 3)
show("-A/B", -A / B)
try:
    A / 0
except ZeroDivisionError:
    show("A / 0", "ZeroDivisionError")

# ----------------------------------------------------------------- the text

for x in [A, C, B, 2**64, 2**100, -(2**100), 0, 10**50]:
    show("str", str(x))
    show("hex", hex(x))
    show("oct", oct(x))
    show("bin", bin(x)[:40])
    show("len(bin)", len(bin(x)))
for s in ["0x" + "f" * 40, "0b" + "1" * 100, "0o777777777777777777777777",
          "123456789" * 5, "-" + "9" * 40, "  12345678901234567890  ",
          "1_000_000_000_000_000_000_000"]:
    show("int(" + s.strip()[:20] + "...)", int(s, 0) if s.strip()[0] in "0-" or
         s.strip()[:2] in ("0x", "0b", "0o") else int(s))
for bad in ["", " ", "0x", "1_", "_1", "1__2", "0b2", "abc", "12a", "01"]:
    try:
        show("int(" + repr(bad) + ", 0)", int(bad, 0))
    except ValueError:
        show("int(" + repr(bad) + ", 0)", "ValueError")
show("int('ff', 16)", int("ff", 16))
show("int('zz', 36)", int("zz", 36))
show("int('-ff', 16)", int("-ff", 16))

# ---------------------------------------------------------------- the power

for b, e in [(2, 100), (3, 64), (-2, 65), (10, 30), (0, 0), (1, 1000), (-1, 999),
             (2, 0), (7, 1)]:
    show(str(b) + "**" + str(e), b**e)
show("2**-2", 2**-2)
show("pow(2, 200, A)", pow(2, 200, A))
show("pow(B, 100, A)", pow(B, 100, A))
show("pow(2, 0, 1)", pow(2, 0, 1))
show("pow(2, 10, -7)", pow(2, 10, -7))

# -------------------------------------------------------------- the methods

for x in [A, C, 2**64, 0, 1, -1, 255, -256]:
    show("bit_length", x.bit_length())
    n = max(1, (x.bit_length() + 8) // 8)
    show("to_bytes/from_bytes", int.from_bytes(x.to_bytes(n, "big", signed=True),
                                               "big", signed=True) == x)
show("as_integer_ratio", A.as_integer_ratio())
show("(0.1).as_integer_ratio", (0.1).as_integer_ratio())
show("(1e300).as_integer_ratio()[1]", (1e300).as_integer_ratio()[1])
show("(2.5).as_integer_ratio", (2.5).as_integer_ratio())
show("(-0.0).as_integer_ratio", (-0.0).as_integer_ratio())
try:
    (2**100).to_bytes(4, "big")
except OverflowError:
    show("to_bytes too small", "OverflowError")
try:
    (-1).to_bytes(4, "big")
except OverflowError:
    show("negative unsigned", "OverflowError")

# ------------------------------------------------------- hashing and rounding

show("hash agrees with float", hash(2**40) == hash(float(2**40)))
show("hash of a small promoted back", hash((2**64) // (2**64)) == hash(1))
show("a big in a dict", {A: "v"}[A])
show("a big in a set", A in {A, B})
for n in [0, -1, -2, -5, -30, 1, 3]:
    show("round(A, " + str(n) + ")", round(A, n))
show("round(2**64, -1)", round(2**64, -1))
show("round(15, -1)", round(15, -1))
show("round(25, -1)", round(25, -1))
show("round(-15, -1)", round(-15, -1))
