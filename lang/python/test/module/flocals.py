# f_locals of a function frame: a FrameLocalsProxy over its slots and cells.
import sys
def f(a, b=2):
    c = 3
    d = None
    del d
    x = 7
    def g():
        return x
    L = sys._getframe().f_locals
    print(type(L).__name__, type(L).__module__, len(L), list(L), 'd' in L, L['a'])
    print(L.get('d', 'no'), sorted(L.keys()), repr(L).split(" at ")[0][:20])
    L['c'] = 30
    print(c)
    L['x'] = 70
    print(g())
    L['a'] = 10
    print(a)
    L['new'] = 1
    print(L['new'], 'new' in L, len(L))
    for k in ('a', 'new', 'zz', 'd'):
        try:
            del L[k]
            print('deleted', k)
        except Exception as e:
            print(type(e).__name__, e)
    for k in ('zz', 1, []):
        try:
            L[k]
        except Exception as e:
            print(type(e).__name__, e)
    L[1] = 2
    print(L[1], 1 in L)
    del L[1]
    print(L == dict(L), dict(L) == L, L.copy() == dict(L), type(L.copy()).__name__)
    print(list(L.items())[:2], list(L.values())[:2], type(L.keys()).__name__)
    print(sys._getframe().f_locals is L, sys._getframe().f_locals == L)
    print(type(L | {}).__name__, ({'z': 1} | L)['z'], (L | {'z': 1})['z'])
    L.update({'q': 5})
    print(L['q'], L.setdefault('c', 0), L.pop('q'), L.pop('zz', 9))
    for bad in (lambda: L.pop('a'), lambda: L.pop('zz'), lambda: hash(L), lambda: type(L)(),
                lambda: type(L)(1), lambda: L.update(q=1), lambda: L.update(1), lambda: L.get(),
                lambda: L.pop(), lambda: L.setdefault(), lambda: L.copy(1), lambda: L | 1,
                lambda: L.clear(), lambda: L < L):
        try:
            bad()
        except Exception as e:
            print(type(e).__name__, e)
    print(list(reversed(L))[:2], isinstance(L, dict), bool(L), type(locals()).__name__)
    print(type(L)(sys._getframe()) == L)
    L['new2'] = 3
    print('new2' in locals(), vars().get('new2'), L != {}, L == 1)
    print(L.setdefault('n'), L['n'])
    L.update(L)
f(1)
print(type(sys._getframe().f_locals).__name__)
class C:
    print(type(sys._getframe().f_locals).__name__)
print((lambda: type(sys._getframe().f_locals).__name__)())
def h():
    y = 1
    L = sys._getframe().f_locals
    L['y'] = 5
    return y, vars().get('y'), eval('y')
print(h())
def k(a):
    def inner():
        return a
    return sys._getframe().f_locals
print(list(k(1)))
def m():
    def inner():
        return z
    L = sys._getframe().f_locals
    print(list(L), repr(L).count('{...}'))
    z = 1
    print(inner(), 'z' in L, list(L.copy()))
m()
def gen():
    v = 1
    yield sys._getframe().f_locals
    v = 2
    yield
it = gen()
L = next(it)
print(L['v'])
next(it)
print(L['v'])
match L:
    case {'v': w}:
        print('matched', w)

# `|=` keeps the proxy, and the dict `|` it stands on (PEP 584).
def ior():
    a = 1
    L = sys._getframe().f_locals
    L |= {"w": 2}
    print(type(L).__name__, L["w"])
ior()
d = {1: 2}
e = d
d |= {3: 4}
print(e, d is e)
d |= [(5, 6)]
print(e)
try:
    d |= 1
except TypeError as x:
    print(x)
try:
    d |= [(1, 2, 3)]
except ValueError as x:
    print(x)
print({1: 2} | {1: 3, 4: 5}, dict.__or__({}, {2: 3}), {}.__or__([]))
try:
    {} | []
except TypeError as x:
    print(x)
class D(dict):
    pass
print(type(D(a=1) | {'b': 2}).__name__, D(a=1) | {'b': 2})
