# pickle and copy over the native types: what each reduces to, the bytes the
# pure-Python pickler writes for it, and the round trip at every protocol.
# pickle._dumps is used so that CPython runs the same pickler this one has.
import array
import collections
import copy
import functools
import io
import operator
import pickle
import pickletools
import types


def show(v):
    if isinstance(v, tuple):
        return "(" + ", ".join(show(x) for x in v) + ("," if len(v) == 1 else "") + ")"
    if isinstance(v, list):
        return "[" + ", ".join(show(x) for x in v) + "]"
    if isinstance(v, dict):
        return "{" + ", ".join(show(k) + ": " + show(x) for k, x in v.items()) + "}"
    if type(v).__name__.endswith("iterator"):
        return "iter" + show(list(v))
    if callable(v) and hasattr(v, "__qualname__"):
        return v.__module__ + "." + v.__qualname__
    return repr(v)


class Point:
    def __init__(self, x, y):
        self.x, self.y = x, y

    def __eq__(self, other):
        return type(other) is Point and (self.x, self.y) == (other.x, other.y)


class Sub(list):
    pass


class MyFloat(float):
    pass


def f(x, y=1):
    return x + y


values = [
    None, True, 0, -1, 255, 65536, 2**31, -2**63, 10**30, 1.5, 1 + 2j, "héllo", b"\x00\xff",
    bytearray(b"ab"), (), (1, 2, 3), [1, [2]], {"a": 1}, {3}, frozenset({4}), range(1, 9, 2),
    slice(1, None, 3), collections.deque([1, 2], 5), collections.OrderedDict(a=1),
    collections.defaultdict(list, a=[1]), collections.Counter("abca"), array.array("i", [1, -2]),
    types.SimpleNamespace(a=1), Point(1, 2), Sub([1, 2]), MyFloat(2.5), f, len, Point, int,
    functools.partial(f, 1, y=2), operator.itemgetter(1), NotImplemented, ...,
]

print("reduce")
for name, v in (
    ("set", {3}), ("range", range(1, 9, 2)), ("slice", slice(1, None, 3)),
    ("bytearray", bytearray(b"ab")), ("deque", collections.deque([1, 2], 5)),
    ("defaultdict", collections.defaultdict(list, a=1)), ("array", array.array("i", [1])),
    ("ValueError", ValueError("x", 2)), ("OSError", OSError(2, "no", "f")),
    ("namespace", types.SimpleNamespace(a=1)), ("partial", functools.partial(f, 1, y=2)),
    ("itemgetter", operator.itemgetter(1, 2)), ("attrgetter", operator.attrgetter("a.b")),
    ("list iterator", iter([1, 2])), ("enumerate", enumerate("ab")),
    ("reversed", reversed([1, 2])), ("zip", zip("a", "b")), ("bound builtin", [].append),
    ("unbound builtin", str.index),
):
    print(name, show(v.__reduce_ex__(2)))

print("bytes")
for v in (None, 1, -1, 300, 2**40, 1.5, "a", b"b", (1, "x"), [1, 2], {"k": [3]}, {5}, Point(1, 2)):
    for proto in (0, 2, 4):
        print(proto, pickle._dumps(v, proto))

print("round trip")
for proto in range(pickle.HIGHEST_PROTOCOL + 1):
    bad = []
    for v in values:
        w = pickle.loads(pickle._dumps(v, proto))
        same = w is v if isinstance(v, (type, types.FunctionType, types.BuiltinFunctionType)) \
            or v in (NotImplemented, ...) else type(w) is type(v) and (
                w.func is v.func if isinstance(v, functools.partial) else
                repr(w) == repr(v) if isinstance(v, operator.itemgetter) else w == v)
        if not same:
            bad.append(repr(v))
    print(proto, bad or "ok")

rec = [1]
rec.append(rec)
w = pickle.loads(pickle._dumps(rec, 2))
print("recursive", w[1] is w)

out = io.StringIO()
pickletools.dis(pickle._dumps({"a": [1, 2]}, 2), out)
print(out.getvalue(), end="")

print("copy")
x = {"a": [1, {2}], "b": (Point(1, 2), bytearray(b"c"))}
y = copy.deepcopy(x)
print(y == x, y["a"] is not x["a"], y["b"][0] is not x["b"][0])
