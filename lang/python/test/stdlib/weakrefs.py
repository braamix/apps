# _weakref.ref is a type weakref.py subclasses, and a subclass's callback
# is handed the instance.
import _weakref, gc
ref = _weakref.ref
print(ref is _weakref.ReferenceType, ref.__name__, ref.__module__)
class A:
    def m(self): return 1
a = A()
r = ref(a)
print(repr(r).split(";")[1][:8], r() is a, r.__callback__, ref(a) is r, hash(r) == hash(ref(a)))
got = []
class K(ref):
    __slots__ = ("key",)
    def __new__(type, ob, callback, key):
        self = ref.__new__(type, ob, callback)
        self.key = key
        return self
    def __init__(self, ob, callback, key):
        super().__init__(ob, callback)
b = A()
k = K(b, lambda wr: got.append((type(wr).__name__, wr.key, wr())), "kk")
print(k() is b, k.key, type(k).__name__, k == ref(b), ref(b) == k, isinstance(k, ref), k.__callback__ is not None)
del b
gc.collect()
print(got, k())
class W(ref):
    def __call__(self):
        obj = super().__call__()
        return ("W", obj is not None)
    __hash__ = ref.__hash__
w = W(a)
print(w(), w is not r, hash(w) == hash(r), ref.__eq__(w, r))
for bad in (1, "s", (1,), [1], {}):
    try:
        ref(bad)
    except TypeError as e:
        print(e)
print(type(ref(A.m)).__name__, ref({1}) is not None, ref(len)() is len)
try:
    class R(ref):
        def __init__(self, ob, cb=None, extra=0):
            super().__init__(ob, cb)
    R(a, None, 5)
except TypeError as e:
    print(e)
