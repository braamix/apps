# collections and collections.abc, as CPython's own modules run them.
from collections import (namedtuple, deque, Counter, OrderedDict, defaultdict, ChainMap,
                         UserDict, UserList, UserString)
from collections.abc import Mapping, MutableMapping, Sequence, Iterable, Hashable, Set

Point = namedtuple("Point", "x y")
p = Point(1, y=2)
print(p, p.x + p.y, p._replace(x=5), p._asdict(), Point._fields, Point._make([3, 4]))
print(isinstance(p, tuple), hash(p) == hash((1, 2)), Point.__doc__)
c = Counter("abracadabra")
print(c.most_common(2), c["a"], c["z"], sorted(c.elements())[:4], c.total())
c.update("aa")
print(c["a"], (c - Counter("a" * 7))["a"], sorted((c & Counter("abc")).items()))
d = deque([1, 2, 3], maxlen=4)
d.appendleft(0)
d.append(4)
print(d, d.maxlen, list(reversed(d)))
od = OrderedDict(a=1, b=2)
od.move_to_end("a")
print(od, list(od), od.popitem(last=False))
dd = defaultdict(list)
dd["k"].append(1)
print(dd, dd.default_factory)
cm = ChainMap({"a": 1}, {"a": 2, "b": 3})
print(cm["a"], cm["b"], len(cm), sorted(cm), cm.new_child({"a": 0})["a"])


class UD(UserDict):
    def __missing__(self, key):
        return key * 2


u = UD(x=1)
print(u["x"], u["yy"], "x" in u, len(u), dict(u))
ul = UserList([3, 1, 2])
ul.sort()
print(ul, ul + [4], ul[1:])
us = UserString("hello")
print(us.upper(), us[1:3], us + "!", len(us))
print(isinstance({}, Mapping), isinstance({}, MutableMapping), isinstance([], Sequence),
      isinstance(frozenset(), Set), issubclass(list, Hashable), isinstance(iter([]), Iterable))


class M(Mapping):
    def __init__(self, d):
        self.d = d

    def __getitem__(self, k):
        return self.d[k]

    def __iter__(self):
        return iter(self.d)

    def __len__(self):
        return len(self.d)


m = M({"a": 1, "b": 2})
print(m.get("a"), m.get("c", 0), "b" in m, list(m.items()), m == {"a": 1, "b": 2}, dict(m))
print(sorted(m.keys()), list(m.values()))
