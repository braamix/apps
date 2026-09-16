# The descriptor protocol, __slots__, and the three attribute hooks.
#
# The same program on CPython and here, byte for byte. So nothing in it may
# depend on when an object is collected, or on a module this interpreter has
# not got.


# A data descriptor comes before the instance namespace; a non-data one after.
class Data:
    def __init__(self, name):
        self.name = name

    def __get__(self, obj, cls):
        if obj is None:
            return 'class ' + self.name
        return obj.__dict__.get('_' + self.name, 'unset')

    def __set__(self, obj, value):
        print('set', self.name, value)
        obj.__dict__['_' + self.name] = value

    def __delete__(self, obj):
        print('del', self.name)
        obj.__dict__.pop('_' + self.name, None)


class Plain:
    def __get__(self, obj, cls):
        return 'plain'


class C:
    d = Data('d')
    p = Plain()


c = C()
print(C.d, C.p)
print(c.d)
c.d = 7
print(c.d, c.__dict__)
del c.d
print(c.d)

# The instance namespace wins over a non-data descriptor and loses to a data one.
print(c.p)
c.__dict__['p'] = 'shadowed'
print(c.p)
c.__dict__['d'] = 'ignored'
print(c.d)


# property is a data descriptor like any other, and reachable on the class.
class P:
    def __init__(self):
        self._v = 0

    @property
    def v(self):
        "the doc"
        return self._v

    @v.setter
    def v(self, x):
        self._v = x * 2

    @v.deleter
    def v(self):
        self._v = -1


p = P()
p.v = 21
print(p.v)
del p.v
print(p.v)
print(type(P.v).__name__, P.v.__doc__)
print(P.v.fget is not None, P.v.fset is not None, P.v.fdel is not None)


# A descriptor written as a class, reached through a subclass.
class Counter:
    def __set_name__(self, owner, name):
        self.name = name

    def __get__(self, obj, cls):
        return (self.name, cls.__name__)


class Base:
    n = Counter()


class Sub(Base):
    pass


print(Sub().n, Base.n)


# __slots__: the names are member descriptors and there is no instance dict.
class S:
    __slots__ = ('a', 'b')

    def __init__(self, a):
        self.a = a


s = S(1)
print(s.a)
try:
    print(s.b)
except AttributeError as e:
    print('AttributeError')
s.b = 2
print(s.a, s.b)
del s.b
try:
    print(s.b)
except AttributeError:
    print('AttributeError')
try:
    s.c = 3
except AttributeError:
    print('no such slot')
try:
    s.__dict__
except AttributeError:
    print('no dict')
print(type(S.a).__name__)


# Slots inherit: the subclass's array starts where the base's ended.
class T(S):
    __slots__ = ('c',)


t = T(10)
t.b = 20
t.c = 30
print(t.a, t.b, t.c)


# A subclass that does not declare __slots__ gets a dict again.
class U(S):
    pass


u = U(1)
u.anything = 2
print(u.a, u.anything)


# __getattribute__, __setattr__ and __delattr__ as hooks, with the defaults
# reachable through super() -- which is how one writes a hook that still stores.
class Watched:
    def __init__(self):
        object.__setattr__(self, 'store', {})

    def __getattribute__(self, name):
        if name.startswith('_') or name == 'store':
            return object.__getattribute__(self, name)
        print('get', name)
        return object.__getattribute__(self, 'store').get(name, 'missing')

    def __setattr__(self, name, value):
        print('put', name, value)
        object.__getattribute__(self, 'store')[name] = value

    def __delattr__(self, name):
        print('drop', name)
        object.__getattribute__(self, 'store').pop(name, None)


w = Watched()
w.x = 1
print(w.x)
del w.x
print(w.x)


# __getattr__ answers only what __getattribute__ could not.
class Fallback:
    def __init__(self):
        self.here = 1

    def __getattr__(self, name):
        return 'fallback:' + name


f = Fallback()
print(f.here, f.elsewhere)
print(getattr(f, 'anything'), getattr(f, 'x', 'dflt'), hasattr(f, 'zzz'))


# Both hooks at once: __getattr__ still gets its turn after an AttributeError.
class Both:
    def __getattribute__(self, name):
        if name == 'ok':
            return 'direct'
        raise AttributeError(name)

    def __getattr__(self, name):
        return 'after:' + name


b = Both()
print(b.ok, b.other)
print(getattr(b, 'q', 'dflt2'), hasattr(b, 'anything'))


# getattr and setattr reach the same machinery the syntax does.
setattr(c, 'd', 5)
print(getattr(c, '_d'))
print(hasattr(p, 'v'), getattr(p, 'nope', 'none of it'))


# __hash__ = None, and an __eq__ without a __hash__, which means the same.
class Eq:
    def __eq__(self, other):
        return True


class NoHash:
    __hash__ = None


for cls in (Eq, NoHash):
    try:
        hash(cls())
    except TypeError:
        print(cls.__name__, 'unhashable')


class Both2:
    def __eq__(self, other):
        return isinstance(other, Both2)

    def __hash__(self):
        return 42


print(hash(Both2()), Both2() == Both2())


# Every object answers __class__, and a class answers its own names.
print(c.__class__ is C, C.__class__ is type, (1).__class__ is int)
print(C.__name__, C.__bases__, len(C.__mro__))
print([x.__name__ for x in C.mro()])
