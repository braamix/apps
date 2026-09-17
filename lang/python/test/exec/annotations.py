# PEP 649: annotations are evaluated when something asks for them.

import sys


def sig(a: int, b: "str" = 2, *rest: float, kw: bytes = b"", **more: complex) -> bool:
    pass


print(sig.__annotations__)
print(sig.__annotations__ is sig.__annotations__)
print(sig.__annotate__(1))
try:
    sig.__annotate__(4)
except NotImplementedError:
    print("format 4 is not the compiler's")


def plain():
    pass


print(plain.__annotations__, plain.__annotate__)
plain.__annotations__ = {"x": 1}
print(plain.__annotations__, plain.__annotate__)


# An annotation is not evaluated where it is written, so a name that does not
# exist costs nothing until it is asked for.
def late(x: NoSuchNameAtAll) -> NoSuchNameAtAll:
    pass


print("late was made")
try:
    late.__annotations__
except NameError as e:
    print("late:", e)


# In a function the annotation is dropped entirely, but the name is still a
# local of it.
def dropped():
    y: AlsoMissing = 1
    return y


print(dropped())


def unbound():
    z: int
    return "z" in locals()


print(unbound())


class Plain:
    T = int
    y: T
    z: "Plain"


print(Plain.__annotations__)
print(Plain.__annotations__ is Plain.__annotations__)


class Derived(Plain):
    pass


print(Derived.__annotations__)


class Written:
    __annotations__ = {"by": "hand"}


print(Written.__annotations__)


# A class or a module records only the annotations it reached.
class Reached:
    if 0:
        never: int
    always: int
    for _ in (1,):
        looped: int


print(Reached.__annotations__)


class Method:
    U = float

    def m(self, a: U) -> U:
        pass


print(Method.m.__annotations__)


def outer():
    L = memoryview

    def inner(a: L) -> L:
        pass

    class Inner:
        b: L

    return inner.__annotations__, Inner.__annotations__


print(outer())


def generic[T: int, *Ts, **P](x: T, *args: *Ts) -> T:
    pass


print(generic.__annotations__)
print(generic.__type_params__[0].evaluate_bound(1))


mod: float
if 0:
    skipped: int
here: str
print(sys.modules[__name__].__annotations__)
print(sorted(k for k in globals() if "annot" in k))
