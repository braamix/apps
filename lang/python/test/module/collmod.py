# _collections: the deque that is a real ring buffer, and the two dict
# subclasses built over the dict this interpreter already has.
from _collections import deque, defaultdict, OrderedDict, _count_elements

d = deque([1, 2, 3])
d.append(4)
d.appendleft(0)
print(d, len(d), d[0], d[-1], list(d), 3 in d, 9 in d, bool(d), bool(deque()))
print(d.pop(), d.popleft(), d)
d.rotate()
print(d)
d.rotate(-2)
print(d)
d.rotate(5)
print(d)
d.extend([7, 8])
d.extendleft([9, 10])
print(d, d.count(7), d.index(8), d.copy())
d.remove(7)
d.insert(1, 99)
print(d)
d.reverse()
print(d, list(reversed(list(d))))
d[0] = -1
print(d)
del d[0]
print(d)
d.clear()
print(d, len(d))

w = deque(maxlen=3)
for i in range(6):
    w.append(i)
print(w, w.maxlen)
w.appendleft(-1)
print(w, deque([1, 2, 3, 4], 2))
print(deque([1, 2]) == deque([1, 2]), deque([1, 2]) < deque([1, 3]))
print(deque([1, 2]) > deque([1]), deque() == deque())
print(repr(deque([1], 5)), repr(deque()))
print(deque("abc"), deque(range(3)), list(deque([1, 2]))[::-1])

big = deque()
for i in range(50):
    big.appendleft(i)
print(len(big), big[0], big[-1], list(big)[:3])
big.rotate(25)
print(big[0], big[-1])

dd = defaultdict(list)
dd["a"].append(1)
dd["a"].append(2)
dd["b"].append(3)
print(dd, dd["a"], isinstance(dd, dict), dd.default_factory, len(dd))
counts = defaultdict(int)
for c in "hello":
    counts[c] += 1
print(dict(counts), sorted(counts.items()))
print(defaultdict(None) == {}, dict(dd), list(dd.keys()), "c" in dd)
try:
    defaultdict()["x"]
except KeyError as e:
    print("KeyError", e)
copy = dd.copy()
print(copy, type(copy) is type(dd))

od = OrderedDict()
od["a"] = 1
od["b"] = 2
od["c"] = 3
print(od, list(od), od["b"], len(od))
od.move_to_end("a")
print(list(od))
od.move_to_end("c", last=False)
print(list(od))
print(od.popitem(), od.popitem(last=False), list(od.items()))
o1 = OrderedDict([("a", 1), ("b", 2)])
o2 = OrderedDict([("b", 2), ("a", 1)])
print(o1 == o2, o1 == {"a": 1, "b": 2}, {"a": 1, "b": 2} == o1)
print(dict(o1), o1.get("a"), o1.setdefault("c", 3), list(o1))
del o1["a"]
print(o1, o1.pop("b"), o1)

m = {}
_count_elements(m, "aabbbc")
print(m)
_count_elements(m, "ab")
print(m)
