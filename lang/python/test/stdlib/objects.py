# What object lends: the comparisons, __hash__, and the pickle helpers
# copy and copyreg call.
import copyreg
class A:
    def __init__(s): s.x = 1
class B:
    __slots__ = ("a", "b")
class E:
    def __eq__(s, o): return isinstance(o, E)
a = A()
print(object.__eq__(a, a), object.__eq__(a, 1), object.__ne__(a, 1), object.__lt__(a, a))
print(object.__eq__(1, 1), object.__ne__(1, 2), object.__lt__(1, 2), object.__hash__(a) == hash(a))
print(E.__ne__(E(), E()), object.__ne__(E(), 1), E() != E(), E() != 1)
r = a.__reduce_ex__(4)
print(r[0] is copyreg.__newobj__, r[1] == (A,), r[2], r[3], r[4])
b = B(); b.a = 3
print(b.__reduce_ex__(2)[2], B().__getstate__(), a.__getstate__())
print(a.__reduce_ex__(1)[0].__name__, a.__reduce__()[0].__name__)
class L(list): pass
l = L([1, 2]); l.z = 5
rl = l.__reduce_ex__(3)
print([c.__name__ for c in rl[1]], rl[2], list(rl[3]), rl[4])
class D(dict): pass
print(list(D(k=1).__reduce_ex__(2)[4]))
class G:
    def __getnewargs__(s): return (1, 2)
    def __getstate__(s): return "st"
print(G().__reduce_ex__(2)[1][1:], G().__reduce_ex__(2)[2])
class GX:
    def __getnewargs_ex__(s): return ((1,), {"k": 2})
rx = GX().__reduce_ex__(2)
print(rx[0].__name__, rx[1][1:])
class R:
    def __reduce__(s): return (R, ())
print(R().__reduce_ex__(2)[0].__name__)
try:
    object().__reduce_ex__(2)
    print("object ok")
except TypeError as e:
    print(e)
import _weakref
try:
    _weakref.ref(A).__reduce_ex__(2)
except TypeError as e:
    print(e)
print((1).__reduce_ex__(2)[1], sorted(object.__dir__(a))[:3], A.__subclasshook__(1))
print(hasattr(len, '__hash__'), hasattr(len, '__eq__'), A.__ne__ is object.__ne__, A.__hash__ is object.__hash__)
