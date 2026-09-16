# PEP 560's other half: `list[int]` is a value, and the class it stands for is
# what a subscripted base really derives from. CPython re-exports the type
# from `types`; here the native floor that module stands on is `_types`.
try:
    from _types import GenericAlias, SimpleNamespace
except ImportError:
    from types import GenericAlias, SimpleNamespace

print(list[int], dict[str, int], tuple[()], set[int], frozenset[str])
print(type(list[int]) is GenericAlias, GenericAlias.__name__)
print(isinstance(list[int], GenericAlias), isinstance([], GenericAlias))

a = list[int]
print(a.__origin__, a.__args__, a.__parameters__)
print(a == list[int], a == list[str], a == dict[int, int], hash(a) == hash(list[int]))
print(len({list[int], list[int], list[str]}))
print(dict[str, list[int]], tuple[int, ...])

x = a()
x.append(1)
print(x, type(x), list[int]([2, 3]))


class C(list[int]):
    pass


print(C.__mro__[1:], C([1, 2]), isinstance(C(), list))
print(list[int].__origin__ is list, dict[str, int].__origin__ is dict)
print(a.append is list.append, a.count is list.count)

try:
    list[int][str]
    print("no error")
except TypeError:
    print("TypeError")

n = SimpleNamespace(a=1, b="x")
print(n, n.a, n.b, vars(n))
n.c = [1]
print(n, n == SimpleNamespace(a=1, b="x", c=[1]), n == SimpleNamespace())
del n.a
print(n, sorted(vars(n)))
print(SimpleNamespace(), SimpleNamespace(z=1) == 1)
