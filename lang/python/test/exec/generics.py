# PEP 695: generic functions and classes, type aliases, lazy bounds.


def ident[T](x: T) -> T:
    return x


print(ident(3), ident.__type_params__, ident.__name__, ident.__qualname__)
X, = ident.__type_params__
print(repr(X), X.__bound__, X.__infer_variance__, X.__module__, type(X).__name__)


def many[A: int, B: (int, str), *Ts, **P](a, b=2, *, c=3):
    return a, b, c


A, B, Ts, P = many.__type_params__
print(many(1), many.__defaults__, many.__kwdefaults__, A.__bound__, B.__constraints__, repr(Ts), repr(P))
print(P.args, P.kwargs, [*Ts])


class Box[T]:
    def __init__(self, item: T):
        self.item = item

    def get(self) -> T:
        return self.item

    def map[U](self, f) -> "Box[U]":
        return Box(f(self.item))


b = Box(2)
print(b.get(), b.map(str).get(), Box.__type_params__, Box.__parameters__, Box.__mro__[1:])
print(Box.__orig_bases__, Box[int], Box.map.__type_params__, Box.map.__qualname__)
T, = Box.__type_params__
print(T is Box.__parameters__[0], Box.__name__, Box.__qualname__)


class Pair[K, V = str](dict):
    pass


print(Pair.__mro__[1:], Pair[int], Pair.__type_params__[1].__default__, Pair({"a": 1}))

type Alias = list[int]
type Gen[K, V] = dict[K, V]
print(Alias, Alias.__value__, Alias.__type_params__, Gen, Gen.__value__, Gen.__type_params__, Gen[str, int])
print(type(Alias).__name__, Alias.__name__, Alias.__module__)

type Later = Undefined
try:
    Later.__value__
except NameError as e:
    print("lazy:", e)
Undefined = float
print(Later.__value__)


def bound_later[T: Missing]():
    pass


try:
    bound_later.__type_params__[0].__bound__
except NameError as e:
    print("lazy bound:", e)
Missing = bytes
print(bound_later.__type_params__[0].__bound__)

type Rec = list[Rec] | None
print(Rec.__value__)


class Outer:
    Inner = int
    type Alias2 = Inner

    def meth[T: Inner](self):
        return T


print(Outer.Alias2.__value__, Outer().meth().__bound__)


def outer():
    local = str

    def f[T: local]():
        pass

    type LA = local
    return f.__type_params__[0].__bound__, LA.__value__


print(outer())


@staticmethod
def undecorated[T]():
    pass


class Deco:
    @classmethod
    def build[T](cls, x: T) -> T:
        return cls, x


print(Deco.build(5)[1], Deco.build(5)[0] is Deco)

for code in ["def f[T, T](): pass", "class C[]: pass", "def f[*Ts: int](): pass",
             "def f[**P: int](): pass", "def f[T=int, U](): pass",
             "type A[T: (yield)] = T", "def f[__classdict__](): pass"]:
    try:
        compile(code, "s", "exec")
    except SyntaxError as e:
        print(e.msg)


class Old:
    pass


print(Old.__type_params__, ident.__type_params__ == (X,), (lambda: 0).__type_params__)
