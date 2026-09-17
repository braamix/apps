# functools, as CPython's own module runs it over the native _functools.
import functools
from functools import (partial, reduce, lru_cache, cache, wraps, total_ordering,
                       cached_property, cmp_to_key, singledispatch, partialmethod)

print(reduce(lambda a, b: a * b, range(1, 6)), reduce(max, [3, 9, 2], 0))
calls = []


@lru_cache(maxsize=2)
def sq(n):
    calls.append(n)
    return n * n


print([sq(i) for i in (1, 2, 1, 3, 1)], calls, sq.cache_info())


@cache
def fib(n):
    return n if n < 2 else fib(n - 1) + fib(n - 2)


print(fib(30))


def deco(fn):
    @wraps(fn)
    def inner(*a):
        return fn(*a) + 1
    return inner


@deco
def named(x):
    "doc"
    return x


print(named(1), named.__name__, named.__doc__, named.__wrapped__(1))


@total_ordering
class V:
    def __init__(self, v):
        self.v = v

    def __eq__(self, o):
        return self.v == o.v

    def __lt__(self, o):
        return self.v < o.v


print(V(1) <= V(2), V(3) >= V(2), V(1) > V(2), sorted([V(3), V(1)])[0].v)


class C:
    runs = 0

    @cached_property
    def value(self):
        C.runs += 1
        return 42


c = C()
print(c.value, c.value, C.runs)
print(sorted([3, 1, 2], key=cmp_to_key(lambda a, b: b - a)))


@singledispatch
def kind(x):
    return "object"


@kind.register(int)
def _(x):
    return "int"


@kind.register(list)
def _(x):
    return "list"


print(kind(1), kind([]), kind("s"), kind(True))
p = partial(int, base=2)
print(p("101"), p.func is int, p.keywords)


class Cell:
    def set(self, v):
        self.v = v
    set_one = partialmethod(set, 1)


cl = Cell()
cl.set_one()
print(cl.v)
