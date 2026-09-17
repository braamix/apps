# _weakref.proxy: the operations a proxy forwards, the two proxy types, and
# what one says once its referent is gone.
import _weakref
class C:
    x = 1
    def __add__(s, o): return "add"
    def __radd__(s, o): return "radd"
    def __len__(s): return 3
    def __call__(s, *a): return ("called", a)
    def __iter__(s): return iter([1, 2])
    def __str__(s): return "strC"
    def __getitem__(s, k): return ("item", k)
    def __contains__(s, k): return k == 7
    def __bool__(s): return False
    def meth(s, y): return ("meth", y)
c = C()
p = _weakref.proxy(c)
print(type(p).__name__, str(p), p + 1, 1 + p, len(p), p(1), list(p), p.x, p.meth(2), p[5], 7 in p, bool(p))
print(_weakref.proxy(c) is p, _weakref.ref(c) is _weakref.ref(c), _weakref.ProxyType, _weakref.CallableProxyType)
print(repr(p).startswith("<weakproxy at 0x"), "to 'C' at 0x" in repr(p), p.__class__ is C)
class D:
    __slots__ = ("a", "b", "__weakref__")
d = D()
q = _weakref.proxy(d)
print(type(q).__name__)
try:
    hash(q)
except TypeError as e:
    print(e)
q.a = 5
print(d.a, q.a, getattr(q, "b", "none"), hasattr(q, "a"))
del q.a
print(hasattr(d, "a"))
n = _weakref.proxy(d, lambda r: None)
print(n is not q, _weakref.getweakrefcount(d) >= 2)
del d
import gc
gc.collect()
for f in (lambda: q.a, lambda: bool(q), lambda: len(q), lambda: str(q)):
    try:
        f()
    except ReferenceError as e:
        print(type(e).__name__, e)
print(repr(q).endswith("; dead>"))
for bad in (1, "s", None):
    try:
        _weakref.proxy(bad)
    except TypeError as e:
        print(e)
lst = []
class E:
    pass
e = E()
r = _weakref.proxy(e)
print(r == r, r != 3, issubclass(ReferenceError, Exception))

# _remove_dead_weakref, which weakref.py imports.
import gc
class F:
    pass
c = F()
d = {1: _weakref.ref(c), 2: _weakref.ref(F)}
_weakref._remove_dead_weakref(d, 1)
print(len(d))
del c
gc.collect()
_weakref._remove_dead_weakref(d, 1)
_weakref._remove_dead_weakref(d, 5)
print(sorted(d))
try:
    _weakref._remove_dead_weakref({1: 2}, 1)
except TypeError as e:
    print(e)
