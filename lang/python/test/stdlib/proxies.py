# types.MappingProxyType, which type.__dict__ answers.
from _types import MappingProxyType as MP
d = {'a': 1}
m = MP(d)
print(type(m).__name__, type(m).__module__, m, repr(MP({})), len(m), m['a'], 'a' in m, list(m), m.get('b', 2))
print(m.keys(), m.values(), m.items(), type(m.copy()).__name__, m.copy() is d, m == d, d == m, m == MP(d), m != 1)
d['b'] = 2
print(m, m | {'c': 3}, {'c': 3} | m, type(m | {}).__name__, list(reversed(m)), str(m))
for bad in (lambda: m.__setitem__('a', 1), lambda: MP(1), lambda: MP([1]), lambda: hash(m), lambda: m['z'], lambda: MP()):
    try:
        bad()
    except Exception as e:
        print(type(e).__name__, e)
try:
    m['a'] = 5
except TypeError as e: print(e)
try:
    del m['a']
except TypeError as e: print(e)
try:
    m |= {}
except TypeError as e: print(e)
class C:
    y = 1
print(type(C.__dict__).__name__, C.__dict__['y'], 'y' in vars(C), type(vars(C)).__name__, MP[int])
try:
    C.__dict__['z'] = 1
except TypeError as e: print(e)
print(hash(MP(frozendict(a=1))) == hash(frozendict(a=1)), MP(frozendict(a=1)))
class PM:
    def __getitem__(s, k): return k
    def get(s, k, d=None): return ('get', k, d)
    def keys(s): return ['x']
print(MP(PM()).get('q'), MP(PM()).keys())
