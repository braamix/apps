# Metaclasses, and the hooks a class owes when it is made.
#
# The same program on CPython and here, byte for byte.


# The plain shape: a metaclass with all three of __prepare__, __new__ and
# __init__, and the class keywords reaching every one of them.
class Meta(type):
    @classmethod
    def __prepare__(mcls, name, bases, **kw):
        print('prepare', name, sorted(kw.items()))
        return {'planted': 'by __prepare__'}

    def __new__(mcls, name, bases, ns, **kw):
        print('new', name, sorted(k for k in ns if not k.startswith('__')), sorted(kw))
        cls = super().__new__(mcls, name, bases, ns)
        cls.tagged = True
        return cls

    def __init__(cls, name, bases, ns, **kw):
        print('init', name, sorted(kw))
        super().__init__(name, bases, ns)

    def shout(cls):
        return cls.__name__.upper()


class Thing(metaclass=Meta, colour='blue'):
    value = 1


print(type(Thing) is Meta, Thing.planted, Thing.value, Thing.tagged)
print(Thing.shout())
print(isinstance(Thing, Meta), issubclass(Meta, type))


# A subclass keeps the metaclass, and the metaclass is derived rather than
# chosen where the bases disagree with the default.
class Sub(Thing):
    pass


print(type(Sub) is Meta, Sub.shout())


# type(name, bases, dict) makes the same thing, and its hooks run too.
Made = type('Made', (Thing,), {'extra': 2})
print(type(Made) is Meta, Made.extra, Made.shout())


# __init_subclass__ is told about every class made under its own, and takes the
# keywords the class statement carried.
class Registry:
    subs = []

    def __init_subclass__(cls, label=None, **kw):
        super().__init_subclass__(**kw)
        Registry.subs.append((cls.__name__, label))


class One(Registry, label='first'):
    pass


class Two(Registry):
    pass


class Three(One, label='third'):
    pass


print(Registry.subs)

try:
    class Bad(Registry2):
        pass
except NameError:
    print('NameError')


class Plain:
    pass


try:
    class WithKeyword(Plain, unwanted=1):
        pass
except TypeError:
    print('TypeError for an unconsumed keyword')


# __set_name__ is told where it ended up, in the order the namespace was
# written, and may change the class it is told about.
class Named:
    def __set_name__(self, owner, name):
        print('set_name', owner.__name__, name)
        self.where = (owner.__name__, name)

    def __get__(self, obj, cls):
        return self.where


class Holder:
    first = Named()
    second = Named()


print(Holder().first, Holder().second)


# __class_getitem__ is an implicit classmethod, and the metaclass's own
# __getitem__ comes before it.
class Generic:
    def __class_getitem__(cls, item):
        return (cls.__name__, item)


print(Generic[int])
print(Generic['a', 'b'])


class GetMeta(type):
    def __getitem__(cls, item):
        return 'metaclass ' + str(item)


class Both(metaclass=GetMeta):
    def __class_getitem__(cls, item):
        return 'never reached'


print(Both[1])


# __mro_entries__: a base that is not a class says what to put in its place,
# and the class remembers what was written.
class Stand:
    def __init__(self, *what):
        self.what = what

    def __mro_entries__(self, bases):
        return self.what


class Real:
    def hello(self):
        return 'hello'


stand = Stand(Real)


class Derived(stand):
    pass


print(Derived().hello(), [b.__name__ for b in Derived.__bases__])
print(Derived.__orig_bases__ == (stand,))


# __instancecheck__ and __subclasscheck__ on a metaclass, which is what an
# abstract base class is made of.
class Duckish(type):
    def __instancecheck__(cls, obj):
        return hasattr(obj, 'quack')

    def __subclasscheck__(cls, sub):
        return 'quack' in dir(sub)


class Duck(metaclass=Duckish):
    pass


class Mallard:
    def quack(self):
        return 'quack'


class Stone:
    pass


print(isinstance(Mallard(), Duck), isinstance(Stone(), Duck))
print(issubclass(Mallard, Duck), issubclass(Stone, Duck))
print(isinstance(Mallard(), (Stone, Duck)))


# super() in a metaclass method takes a class as its self and still finds the
# class it was written in.
class Outer(type):
    def where(cls):
        return 'Outer'


class Inner(Outer):
    def where(cls):
        return 'Inner then ' + super().where()


class Uses(metaclass=Inner):
    pass


print(Uses.where())


# A class is an instance of its metaclass and answers as one.
print(Thing.__class__ is Meta, type(type) is type)
print([c.__name__ for c in type(Thing).__mro__])
