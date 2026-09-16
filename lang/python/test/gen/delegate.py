# `yield from`: send, throw and close all go through to the sub-iterator, and
# what the sub-iterator returns is the value of the expression.

def leaf(n):
    for i in range(n):
        sent = yield i
        print("leaf got", sent)
    return "leaf done"


def middle(n):
    got = yield from leaf(n)
    print("middle saw", got)
    yield "after"
    return "middle done"


g = middle(2)
print(next(g))
print(g.send("a"))
print(g.send("b"))
try:
    g.send("c")
except StopIteration as e:
    print("end", e.value)

# Any iterable delegates, not only a generator.
def mixed():
    yield from [1, 2]
    yield from "xy"
    yield from range(2)
    print((yield from ()))
    yield from {"k": 1}


print(list(mixed()))

# A sub-iterator that never yields gives None.
def stopped():
    print((yield from ()))
    print((yield from leaf(0)))


print(list(stopped()))

# Three deep.
def a():
    yield from b()
def b():
    yield from c()
def c():
    yield 1
    yield 2


print(list(a()))
g = a()
print(next(g))
print(next(g))
try:
    next(g)
except StopIteration:
    print("three deep done")


# throw goes to the innermost generator that can take it.
def inner():
    try:
        yield 1
    except KeyError:
        print("inner caught")
        yield 2
    yield 3


def outer():
    try:
        yield from inner()
    except ValueError:
        print("outer caught")
        yield 9


g = outer()
print(next(g))
print(g.throw(KeyError))
print(next(g))

# When the sub-iterator will not take it, the delegator sees it.
def plain():
    try:
        yield from [1, 2]
    except ValueError:
        print("delegator caught")
        yield 7


g = plain()
print(next(g))
print(g.throw(ValueError))


# close shuts the sub-iterator down first, and the delegator still sees a
# GeneratorExit of its own.
def leafclose():
    try:
        yield 1
        yield 2
    except GeneratorExit:
        print("leaf exiting")
        raise


def midclose():
    try:
        yield from leafclose()
    except GeneratorExit:
        print("middle exiting")
        raise


g = midclose()
print(next(g))
print(g.close())

# A sub-iterator with a close method of its own.
class Closable:
    def __iter__(self):
        return self

    def __next__(self):
        return 5

    def close(self):
        print("Closable.close")


def overclosable():
    yield from Closable()


g = overclosable()
print(next(g))
print(g.close())


# A duck-typed sub-iterator: send and throw reach its own methods.
class Duck:
    def __iter__(self):
        return self

    def __next__(self):
        return "next"

    def send(self, v):
        print("Duck.send", v)
        return "sent"

    def throw(self, x):
        print("Duck.throw", x)
        return "thrown"


def overduck():
    yield from Duck()


g = overduck()
print(next(g))
print(g.send("hello"))
print(g.throw(TypeError))


# A generator expression is a generator, and nests like one.
print(list(x * 2 for x in range(4)))
print(sum(x for x in range(5) if x % 2))
print(list(y for y in (x * x for x in range(4))))
print(sorted((x for x in [3, 1, 2]), reverse=True))
print(", ".join(str(x) for x in range(3)))
print(dict((c, i) for i, c in enumerate("abc")))
print(max((x for x in [4, 9, 2]), key=lambda v: -v))
print([*(x for x in range(3))], {*(x for x in range(3))})
first, *rest = (x for x in range(4))
print(first, rest)
print(2 in (x for x in range(4)))


# The frame survives the collector across a suspension.
def deep(n):
    local = [n] * 4
    while n:
        n -= 1
        yield local[0], n


g = deep(3)
for item in g:
    print(item)
