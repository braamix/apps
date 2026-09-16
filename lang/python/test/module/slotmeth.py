# The protocol methods in a built-in type's namespace. `len(x)` reaches a slot
# and a slot is not an entry, so until now `'__len__' in list.__dict__` was
# False and collections.abc's __subclasshook__ -- which decides membership by
# looking exactly these names up -- could not work at all.
print("__len__" in list.__dict__, "__getitem__" in dict.__dict__)
print("__iter__" in set.__dict__, "__contains__" in str.__dict__)
print("__add__" in int.__dict__, "__radd__" in float.__dict__)
print(dict.__hash__, list.__hash__, set.__hash__, bytearray.__hash__)
print(tuple.__hash__ is None, str.__hash__ is None, frozenset.__hash__ is None)

print(hasattr(list, "__and__"), hasattr(set, "__and__"), hasattr(frozenset, "__xor__"))
print(hasattr(list, "__mod__"), hasattr(str, "__mod__"), hasattr(int, "__lshift__"))
print("__lt__" in set.__dict__, "__lt__" in list.__dict__, "__gt__" in str.__dict__)
print(hasattr(list, "__reversed__"), hasattr(int, "__index__"), hasattr(str, "__index__"))
print(hasattr(int, "__invert__"), hasattr(float, "__invert__"), hasattr(float, "__abs__"))

print(list.__len__([1, 2]), dict.__len__({"a": 1}), str.__len__("abc"))
print(str.__contains__("abc", "b"), list.__contains__([1], 1), int.__index__(5))
print(int.__add__(1, 2), int.__add__(1, "x"), int.__mul__(2, 3))
print((3).__neg__(), (-3).__abs__(), (3).__invert__(), (2.5).__abs__())
print(str.__eq__("a", "a"), [1].__eq__([1]), [1].__lt__([2]))
print(list(list.__reversed__([1, 2, 3])), list(dict.__iter__({"a": 1})))
print(tuple.__hash__((1, 2)) == hash((1, 2)), str.__repr__("a"), (1).__repr__())
print([1].__repr__(), {}.__repr__(), (1.5).__bool__(), (0).__bool__(), "".__str__())
print(next(iter([1, 2]).__iter__()), [1, 2].__getitem__(1), "ab".__getitem__(0))

xs = [1, 2, 3]
xs.__setitem__(0, 9)
print(xs)
xs.__delitem__(0)
print(xs)

# A dict subclass may answer for a key it has not got: dict.__getitem__ is
# where __missing__ lives, and so it is here.
class Filled(dict):
    def __missing__(self, key):
        return "made:" + key


f = Filled(a=1)
print(f["a"], f["z"], "z" in f, len(f))


class Counter(dict):
    def __missing__(self, key):
        self[key] = 0
        return 0


c = Counter()
c["x"] += 1
c["x"] += 1
print(c, c["y"], c)


class Plain(dict):
    pass


try:
    Plain()["q"]
except KeyError as e:
    print("KeyError", e)

# A subclass of a built-in keeps the built-in's own `in`, which is a
# subsequence test for bytes and a membership test for a list.
class MyBytes(bytes):
    pass


class MyList(list):
    pass


b = MyBytes(b"1234")
print(b"1" in b, 0 in b, b"23" in b, len(b), b[0])
l = MyList([1, 2, 3])
print(1 in l, 9 in l, len(l), l[1], list(reversed(l)), l + [4])
