# X | Y: the union type, its members, equality and what isinstance makes of it.
import _types
u = int | str
print(type(u), u, u.__args__, u.__parameters__, u.__origin__)
print(u == (str | int), hash(u) == hash(str | int), int | int, type(int | int))
print(int | None, (int | None).__args__, None | int)
print(isinstance(1, int | str), isinstance(1.0, int | str), issubclass(bool, int | str))
print(list[int] | None, type(list[int] | None) is _types.UnionType)
print((int | str) | float, int | (str | float), int | str | int)
try:
    1 | int
except TypeError as e:
    print(e)
try:
    isinstance(1, list[int] | str)
except TypeError as e:
    print(e)
class M(type):
    def __or__(cls, o):
        return "M-or"
class C(metaclass=M):
    pass
print(C | int, int | C)
print(type.__or__(int, str), type.__or__(int, 3))
print(u | None, None | u)
print(list[int] | int, (list[int] | int).__args__)
try:
    class D(int | str):
        pass
except TypeError as e:
    print(e)
try:
    u()
except TypeError as e:
    print(e)
print(u.__name__, u.__qualname__, u.__module__)
print(len({int | str, str | int}), isinstance(3, (int | str, float)))
try:
    u[int]
except TypeError as e:
    print(e)
print(_types.UnionType[int, str], _types.UnionType[int])
try:
    _types.UnionType[()]
except TypeError as e:
    print(e)
class K:
    pass
print(K | None)
try:
    int | ...
except TypeError as e:
    print(e)
print(dict[str, int] | list[int])
