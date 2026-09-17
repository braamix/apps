# The implicit __class__ cell: zero-argument super(), __class__ itself,
# __classcell__ and what type.__new__ and __build_class__ do with it.
import sys
class A:
    def f(self):
        return __class__
    def g(self):
        return super()
    def h(self):
        nonlocal __class__
        __class__ = 5
        return __class__
    def comp(self):
        return [super().__class__ for _ in [0]]
    def gen(self):
        return list(super().__class__ for _ in [0])
    def lam(self):
        return (lambda: super())()
    @classmethod
    def cm(cls):
        return super().__init_subclass__
    @staticmethod
    def sm():
        try:
            return super()
        except Exception as e:
            return type(e).__name__, str(e)
    def noargs():
        try:
            super()
        except Exception as e:
            return type(e).__name__, str(e)
    def deleted(self):
        del self
        try:
            super()
        except Exception as e:
            return type(e).__name__, str(e)
    def star(*args):
        return super().__class__
    def cellarg(self):
        f = lambda: self
        return super().__class__
    try:
        print(__class__)
    except NameError as e:
        print("class body", e)
    x = sys._getframe().f_code.co_cellvars
print('__class__' in A.x, A.f.__code__.co_freevars, A.g.__code__.co_freevars, A.f.__code__.co_cellvars)
print(A().f() is A, A().g().__thisclass__ is A, '__classcell__' in A.__dict__)
print(A.h.__code__.co_freevars)
a = A()
for m in ('comp', 'gen', 'lam', 'cellarg', 'star'):
    try:
        print(m, getattr(a, m)())
    except Exception as e:
        print(m, type(e).__name__, str(e).split(' (')[0])
print(callable(A.cm()), A.sm(), A.noargs(), a.deleted())
def outside(self):
    try:
        super()
    except Exception as e:
        return type(e).__name__, str(e)
print(outside(1))
print(a.h(), A().f())
def outer():
    __class__ = 1
    def inner(self):
        return super()
    return inner
try:
    outer()(1)
except Exception as e:
    print(type(e).__name__, e)
class Meta(type):
    def __new__(m, name, bases, ns):
        ns2 = dict(ns)
        ns2.pop('__classcell__', None)
        return super().__new__(m, name, bases, ns2)
try:
    class B(metaclass=Meta):
        def f(self):
            return __class__
except Exception as e:
    print(type(e).__name__, str(e).replace('__main__.', ''))
class Meta2(type):
    def __new__(m, name, bases, ns):
        return super().__new__(m, name, bases, ns)
class C(metaclass=Meta2):
    def f(self):
        return __class__
print(C().f() is C, C.__dict__.get('__classcell__'))
try:
    type('D', (), {'__classcell__': 1})
except Exception as e:
    print(type(e).__name__, e)
class E:
    def f(self):
        return super().__class__
    print(sorted(k for k in dict(locals()) if k.startswith('__')))
def cell_ns():
    ns = {}
    def g():
        return __class__
    return g
try:
    class F:
        def f(self):
            return __class__
        __classcell__ = 1
except Exception as e:
    print(type(e).__name__, e)
class G:
    def f(self):
        return super().f() if False else __class__
class H(G):
    def f(self):
        return super().f(), __class__
print(H().f() == (G, H))
class I:
    def __init_subclass__(cls, **kw):
        super().__init_subclass__()
        print("init_subclass", cls.__name__, __class__.__name__)
class J(I):
    pass
class K:
    def __new__(cls):
        return super().__new__(cls)
    def __repr__(self):
        return "K via " + super().__repr__()[-1:]
print(repr(K()))
