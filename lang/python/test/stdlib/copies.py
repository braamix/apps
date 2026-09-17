# copy and copyreg over object's __reduce_ex__.
import copy
import copyreg


class P:
    def __init__(self, a):
        self.a = a
        self.items = [a]


p = P(1)
q = copy.copy(p)
r = copy.deepcopy(p)
print(q.a, q.items is p.items, r.items is p.items, r.items == p.items, type(r).__name__)
x = [1, [2, 3], {"k": (4, [5])}]
y = copy.deepcopy(x)
print(y == x, y[1] is x[1], y[2]["k"][1] is x[2]["k"][1])
loop = []
loop.append(loop)
dl = copy.deepcopy(loop)
print(dl[0] is dl, dl is not loop)


class S:
    __slots__ = ("a", "b")


s = S()
s.a = 5
t = copy.copy(s)
print(t.a, hasattr(t, "b"))


class Custom:
    def __init__(self, v):
        self.v = v

    def __copy__(self):
        return Custom(self.v + 100)

    def __deepcopy__(self, memo):
        return Custom(self.v + 200)


print(copy.copy(Custom(1)).v, copy.deepcopy(Custom(1)).v)


class Reduced:
    def __init__(self, v):
        self.v = v

    def __reduce__(self):
        return (Reduced, (self.v * 2,))


print(copy.copy(Reduced(4)).v)


class L(list):
    pass


ll = L([1, 2])
ll.tag = "t"
cl = copy.deepcopy(ll)
print(cl, type(cl).__name__, cl.tag)
print(copy.replace(P(3).__class__(7), a=8).a if hasattr(P, "__replace__") else "no replace")


class Pt:
    def __init__(self, x):
        self.x = x


copyreg.pickle(Pt, lambda o: (Pt, (o.x + 1,)))
print(copy.copy(Pt(1)).x, copyreg._slotnames(S), copy.copy(frozenset({1})) == {1})
