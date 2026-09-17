# weakref.py over _weakref: the dictionaries, WeakSet, WeakMethod and finalize.
import gc
import weakref


class K:
    def __init__(self, n):
        self.n = n

    def m(self):
        return self.n

    def __repr__(self):
        return "K" + str(self.n)


a, b = K(1), K(2)
wv = weakref.WeakValueDictionary({"a": a, "b": b})
wk = weakref.WeakKeyDictionary({a: "x", b: "y"})
ws = weakref.WeakSet([a, b])
print(sorted(wv), wk[a], len(ws), a in ws, wv.get("a") is a)
del b
gc.collect()
print(sorted(wv), len(wk), len(ws), list(wk.values()), list(ws))
wm = weakref.WeakMethod(a.m)
print(wm()(), wm == weakref.WeakMethod(a.m), hash(wm) == hash(weakref.WeakMethod(a.m)))
done = []
f = weakref.finalize(a, done.append, "gone")
print(f.alive, f.peek()[0] is a)
del a
gc.collect()
print(done, f.alive, wm(), list(wv.keys()), len(wk))
r = weakref.ref(K(3))
gc.collect()
print(r(), repr(weakref.ref(K)).startswith("<weakref at "))
c = K(4)
p = weakref.proxy(c)
print(p.n, p.m(), isinstance(p, weakref.ProxyTypes), weakref.ReferenceType is weakref.ref)
