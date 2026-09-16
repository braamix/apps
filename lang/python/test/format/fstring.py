# f-strings. The lexer keeps the body as written and the parser takes it
# apart, so what is tested here is that split as much as the formatting: the
# doubled braces, the escapes in the literal halves, the nesting, and `=`.
#
# Run by CPython to write the golden and by this interpreter to check it.

x = 5
w = 8
p = 3
name = "world"
d = {"k": "v", 1: "one"}
lst = [10, 20]

print(f"")
print(f"plain")
print(f"hello {name}")
print(f"{x}")
print(f"{x} {x}")
print(f"a{x}b{x}c")
print(f"{x + 1}")
print(f"{ x + 1 }")
print(f"{x if x else 0}")
print(f"{[i * 2 for i in lst]}")
print(f"{d['k']} {d[1]}")
print(f"{lst[0]}{lst[-1]}")
print(f"{(1, 2)}")
print(f"{(lambda: 7)()}")

# ------------------------------------------------------------------- braces

print(f"{{}}")
print(f"{{{x}}}")
print(f"{{{{}}}}")
print(f"}}{{")
print(f"a{{b}}c")

# ------------------------------------------------------------- the escapes

print(f"\n{x}".replace("\n", "<NL>"))
print(f"{x}\t".replace("\t", "<TAB>"))
print(rf"\n{x}")
print(f"\x7bx\x7d")
print(f"\\{x}")
print(f"""triple {x}""")
print(f'''{x} single''')

# --------------------------------------------------------- the conversions

print(f"{name!r}")
print(f"{name!s}")
print(f"{'héllo'!a}")
print(f"{name!r:>10}|")
print(f"{name!s:*^11}|")

# ----------------------------------------------------------------- the spec

print(f"{x:5}|")
print(f"{x:<5}|")
print(f"{x:05}")
print(f"{3.14159:.2f}")
print(f"{1234567:,}")
print(f"{x:{w}}|")
print(f"{x:>{w}}|")
print(f"{3.14159:{w}.{p}f}|")
print(f"{x:{'>'}{w}}|")
print(f"{x:}")
print(f"{x:{''}}")

# -------------------------------------------------------------------- the =

print(f"{x=}")
print(f"{x = }")
print(f"{ x = }")
print(f"{x+1=}")
print(f"{x=:5}|")
print(f"{x=!s}")
print(f"{d['k']=}")

# ------------------------------------------------------------ the joining

print("lit" f"{x}" "tail")
print(f"{x}" f"{x}")
print(f"a" "b" f"c")
print("only" "plain")

# ------------------------------------------------------------- the classes


class Own:
    def __format__(self, spec):
        return "OWN(" + spec + ")"

    def __repr__(self):
        return "own-repr"

    def __str__(self):
        return "own-str"


o = Own()
print(f"{o}")
print(f"{o:abc}")
print(f"{o!r}")
print(f"{o!s}")
print(f"{o!r:>12}|")
print(f"{[o]}")
print(f"{[o]!r}")
print(f"{o:{'xy'}}")

# ------------------------------------------------------------ the nesting

print(f"{f'{x}'}")
print(f"{f'{x}' + f'{w}'}")
print(f"{f'{x:{w}}'}")
