"""The code object and the function object, made visible."""


def plain(a, b=2, *rest, kw=3, **more):
    """what plain does"""
    local = a
    return local


# What a function answers to.
print(plain.__name__, plain.__qualname__)
print(plain.__doc__)
print(plain.__module__)
print(plain.__defaults__, plain.__kwdefaults__, plain.__closure__)
print(plain.__globals__ is globals())
print(type(plain.__dict__).__name__, plain.__dict__)


# The code object under it.
c = plain.__code__
print(c.co_name)
print(c.co_argcount, c.co_posonlyargcount, c.co_kwonlyargcount)
print(c.co_varnames)
print(c.co_nlocals)
print(c.co_filename.endswith("attrs.py") or c.co_filename.endswith("c.py"))
print(type(c.co_consts) is tuple, type(c.co_names) is tuple)
print(c.co_cellvars, c.co_freevars)
print(type(c.co_flags) is int, type(c.co_stacksize) is int, type(c.co_firstlineno) is int)


# Nesting, and what the names then look like.
class Outer:
    def method(self):
        def inner():
            pass

        return inner


print(Outer.method.__qualname__)
print(Outer().method().__qualname__)
print(Outer.method.__name__, Outer().method.__name__)
print((lambda: 0).__name__, (lambda: 0).__qualname__)


def capture():
    seen = 1

    def look():
        return seen

    return look


got = capture()
print(got.__code__.co_freevars, len(got.__closure__))
print(capture.__code__.co_cellvars)


# Assignment.
def target():
    pass


target.__name__ = "renamed"
target.__qualname__ = "a.b.renamed"
target.__doc__ = "new doc"
target.__defaults__ = (7,)
print(target.__name__, target.__qualname__, target.__doc__, target.__defaults__)
target.own = 11
print(target.own, target.__dict__)
for name, value in (("__name__", 1), ("__code__", 1), ("__defaults__", 1)):
    try:
        setattr(target, name, value)
        print("no error", name)
    except TypeError:
        print("TypeError", name)
for name in ("__globals__", "__closure__"):
    try:
        setattr(target, name, None)
    except AttributeError:
        print("AttributeError", name)

# A decorator that keeps the wrapped name, which is what __name__ is for.
def keep(fn):
    def wrapper(*a):
        return fn(*a)

    wrapper.__name__ = fn.__name__
    wrapper.__qualname__ = fn.__qualname__
    wrapper.__doc__ = fn.__doc__
    return wrapper


@keep
def decorated(v):
    """kept"""
    return v * 2


print(decorated.__name__, decorated.__qualname__, decorated.__doc__, decorated(21))


# A code object made callable over a namespace of one's own.
def reads_global():
    return value


maker = type(reads_global)
one = maker(reads_global.__code__, {"value": 1})
two = maker(reads_global.__code__, {"value": 2})
print(one(), two(), type(one) is maker)
print(one.__name__, one.__globals__ is two.__globals__)
named = maker(reads_global.__code__, {"value": 3}, "chosen")
print(named.__name__, named())
try:
    maker(None, {})
except TypeError:
    print("TypeError code")
try:
    maker(reads_global.__code__, None)
except TypeError:
    print("TypeError globals")


# Docstrings on the other two scopes.
class Documented:
    """a class doc"""


class Undocumented:
    pass


print(__doc__, Documented.__doc__, Undocumented.__doc__)


# A built-in, which has a name and not much else.
print(len.__name__, [].append.__name__)
print(type(len).__name__)
