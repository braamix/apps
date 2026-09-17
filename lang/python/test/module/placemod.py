# _functools: partial with Placeholder, and cmp_to_key.
import _functools
from _functools import partial, Placeholder as P, _PlaceholderType as PT, cmp_to_key
print(sorted(n for n in dir(_functools) if not n.startswith('__')))
print(repr(P), type(P) is PT, PT() is P, PT.__name__, PT.__module__, P.__reduce__())
print(type(partial), partial.__name__, partial.__module__)
try:
    PT(1)
except TypeError as e:
    print(e)
try:
    class X(PT):
        pass
except TypeError as e:
    print(e)
def f(*a, **k):
    return a, k
p = partial(f, P, 2, P, 3, x=1)
print(p(10, 30, 40, y=2), p.args, p.keywords)
print(repr(p).replace(repr(f), "F"))
try:
    p(1)
except TypeError as e:
    print(e)
q = partial(p, 5)
print(q.args, q(6, 7), q.func is f)
print(partial(p, P, 8).args, partial(q, 9).args, partial(q, 9)())
for bad in (lambda: partial(f, 1, P), lambda: partial(f, x=P), lambda: partial(f, P),
            lambda: partial(p, P, P), lambda: partial(), lambda: partial(1)):
    try:
        bad()
    except TypeError as e:
        print(e)
print(partial(f, 1)(2), partial(partial(f, 1, a=1), 2, a=3).keywords,
      partial(partial(f, 1), 2).args, partial(f, x=1).keywords)
g = partial(f, 1)
g.attr = 5
print(partial(g, 2).func is g, partial(g, 2).args)
print(bool(P), P == P, hash(P) == hash(P))
K = cmp_to_key(lambda a, b: (a > b) - (a < b))
print(sorted([3, 1, 2], key=K), K(1) < K(2), K(1) == K(1), K(2) >= K(3), K(1) != K(1), K(2) > K(1),
      K(2) <= K(2))
print(type(K).__name__, type(K(1)).__name__, type(K) is type(K(1)), K(1).obj, K(obj=4).obj)
for bad in (lambda: hash(K(1)), lambda: K(1) < 2, lambda: K(), lambda: cmp_to_key(),
            lambda: sorted([1, "a"], key=cmp_to_key(lambda a, b: (a > b) - (a < b)))):
    try:
        bad()
    except TypeError as e:
        print(type(e).__name__, str(e).replace("functools.", ""))
print(cmp_to_key(mycmp=lambda a, b: 0)(1) == cmp_to_key(lambda a, b: 1)(2))
print(sorted("bca", key=cmp_to_key(lambda a, b: -1 if a < b else 1), reverse=True))
print(max([4, 9, 2], key=cmp_to_key(lambda a, b: b - a)), min("xyz", key=cmp_to_key(lambda a, b: 0)))
class N:
    def __lt__(s, o):
        return "neg"
K3 = cmp_to_key(lambda a, b: N())
print(K3(1) < K3(2))
class S(partial):
    pass
print(S(f, 1)(2), isinstance(S(f), partial))
lst = [5, 3, 4]
lst.sort(key=cmp_to_key(lambda a, b: a - b))
print(lst)
