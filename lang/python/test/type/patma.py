# A match statement takes an ABC's instances for sequences and mappings.
import abc
class Seq(metaclass=abc.ABCMeta):
    __abc_tpflags__ = 1 << 5
class Map(metaclass=abc.ABCMeta):
    __abc_tpflags__ = 1 << 6
class MySeq(Seq):
    def __len__(self): return 3
    def __getitem__(self, i):
        if i >= 3: raise IndexError(i)
        return i * 10
class Other:
    def __init__(self): self.d = {"a": 1, "b": 2}
    def __len__(self): return len(self.d)
    def get(self, k, default=None): return self.d.get(k, default)
    def keys(self): return self.d.keys()
    def __getitem__(self, k): return self.d[k]
    def __iter__(self): return iter(self.d)
Map.register(Other)
print('__abc_tpflags__' in Seq.__dict__)
for v in [MySeq(), Other(), object()]:
    match v:
        case [a, b, c]:
            print("seq", a, b, c)
        case {"a": x, **rest}:
            print("map", x, rest)
        case _:
            print("neither")
match MySeq():
    case [first, *_]:
        print("first", first)
