# match: every pattern kind, guards, captures and the failures they report.


class Point:
    __match_args__ = ("x", "y")

    def __init__(self, x, y):
        self.x = x
        self.y = y


class Plain:
    def __init__(self):
        self.a = 1


def show(v):
    match v:
        case None:
            return "none"
        case True:
            return "true"
        case 0 | 1 as small:
            return f"small {small}"
        case -1:
            return "minus one"
        case 1.5 | -2.5:
            return "half"
        case 1 + 2j:
            return "complex"
        case "hi" | b"hi":
            return f"greeting {v!r}"
        case []:
            return "empty list"
        case [x]:
            return f"one {x}"
        case [x, y] if x == y:
            return f"pair of {x}"
        case [x, y]:
            return f"pair {x} {y}"
        case [first, *rest] if len(rest) > 3:
            return f"long {first} {rest}"
        case [first, *_, last]:
            return f"ends {first} {last}"
        case {"kind": "circle", "r": r}:
            return f"circle {r}"
        case {"kind": k, **others}:
            return f"kind {k} {sorted(others.items())}"
        case {}:
            return "some dict"
        case Point(0, 0):
            return "origin"
        case Point(x, 0):
            return f"on x {x}"
        case Point(x=0, y=yy):
            return f"on y {yy}"
        case Point():
            return "a point"
        case int(n) if n > 100:
            return f"big {n}"
        case str() as s:
            return f"str {s}"
        case (a, b, c):
            return f"triple {a} {b} {c}"
        case _:
            return "other"


for v in [None, True, False, 0, 1, -1, 1.5, -2.5, 1 + 2j, "hi", b"hi", [], [5], [2, 2],
          [2, 3], [1, 2, 3, 4, 5], [1, 2, 3], (1, 2, 3), {"kind": "circle", "r": 2},
          {"kind": "sq", "a": 1, "b": 2}, {"x": 1}, Point(0, 0), Point(3, 0), Point(0, 4),
          Point(5, 6), 200, 50, "text", range(3), 2.0, {1, 2}]:
    print(repr(v) if not isinstance(v, Point) else "Point", "->", show(v))


def cls(v):
    match v:
        case Plain(a=1):
            return "plain"
        case bool(b):
            return f"bool {b}"
        case float(f) | int(f):
            return f"number {f}"
        case list([x, *_]):
            return f"list starting {x}"
        case dict({"k": value}):
            return f"dict {value}"
        case tuple((1, *more)):
            return f"tuple {more}"
    return "none"


for v in [Plain(), True, 3, 2.5, [7, 8], {"k": "v"}, (1, 2, 3), (2,), "s"]:
    print(cls(v))


def errors(f):
    try:
        print(f())
    except TypeError as e:
        print("TypeError:", e)
    except ValueError as e:
        print("ValueError:", e)


class NoArgs:
    pass


class BadArgs:
    __match_args__ = ["x"]


class Dup:
    __match_args__ = ("x", "x")
    x = 1


def m1():
    match NoArgs():
        case NoArgs(1):
            return "no"


def m2():
    match BadArgs():
        case BadArgs(1):
            return "no"


def m3():
    match 1:
        case len(1):
            return "no"


def m4():
    match Dup():
        case Dup(1, 1):
            return "no"


def m6():
    match 3:
        case int(1, 2):
            return "no"


for f in [m1, m2, m3, m4, m6]:
    errors(f)

# subject forms, and names bound only on success
match 1, 2:
    case (a, b):
        print("tuple subject", a, b)
match [1, 2, 3]:
    case [1, x, 4] | [1, 2, x]:
        print("or captures", x)
match {"a": [1, {"b": (2, 3)}]}:
    case {"a": [1, {"b": (p, q)}]}:
        print("nested", p, q)
try:
    zz
except NameError:
    print("zz unbound")
match [1, 2]:
    case [zz, 3]:
        pass
    case _:
        pass
try:
    print(zz)
except NameError:
    print("zz still unbound")
match 5:
    case x if x > 10:
        print("big")
    case x:
        print("fallback", x)
for i in range(3):
    match i:
        case 0:
            continue
        case 1:
            print("one")
        case _:
            break
print("loop done")


def ret(v):
    for item in v:
        match item:
            case "stop":
                return "stopped"
    return "ran out"


print(ret(["a", "stop", "b"]), ret(["a"]))
match "abc":
    case [*chars]:
        print("str is not a sequence")
    case str():
        print("str matched as str")
