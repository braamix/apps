# The coroutine object: how a plain caller drives one, what it says about
# itself, and every way awaiting can be refused.

import _types


def drive(c):
    try:
        while True:
            print("  yielded", c.send(None))
    except StopIteration as e:
        print("  returned", e.value)
    except BaseException as e:
        print("  raised", type(e).__name__, e)


# types.coroutine, as types.py writes it, over code.replace().
def iterable(f):
    f.__code__ = f.__code__.replace(co_flags=f.__code__.co_flags | 0x100)
    return f


@iterable
def tick(label):
    got = yield label
    return (label, got)


async def leaf(n):
    return n * 2


async def chain(n):
    if n == 0:
        return await tick("bottom")
    return await chain(n - 1)


print("-- send, return and reuse")
c = leaf(21)
print(type(c).__name__, c.__name__, c.__qualname__)
print(c.cr_running, c.cr_await, c.cr_suspended, c.cr_code.co_name, c.cr_origin)
drive(c)
drive(c)
print(c.cr_frame, c.close())

print("-- a chain of awaits suspends at the bottom")
c = chain(3)
print(c.send(None))
print(c.cr_suspended, type(c.cr_await).__name__)
try:
    c.send("answer")
except StopIteration as e:
    print("returned", e.value)

print("-- throw and close")


async def guarded():
    try:
        await tick("waiting")
    except KeyError as e:
        print("  caught", repr(e))
        return "recovered"
    finally:
        print("  finally")


c = guarded()
print(c.send(None))
try:
    c.throw(KeyError("k"))
except StopIteration as e:
    print("returned", e.value)

c = guarded()
print(c.send(None))
print(c.close())
drive(c)

c = leaf(1)
try:
    c.throw(ValueError("early"))
except ValueError as e:
    print("unstarted throw:", e)
drive(c)

c = leaf(1)
try:
    c.send(5)
except TypeError as e:
    print(e)
c.close()


async def stubborn():
    try:
        await tick("x")
    except GeneratorExit:
        await tick("y")


c = stubborn()
c.send(None)
try:
    c.close()
except RuntimeError as e:
    print(e)
drive(c)

print("-- __await__ is how a plain iterator reaches one")
w = leaf(4).__await__()
print(type(w).__name__, iter(w) is w, hasattr(w, "__await__"))
try:
    next(w)
except StopIteration as e:
    print("next ->", e.value)

w = chain(1).__await__()
print(next(w), end=" ")
try:
    w.send("sent")
except StopIteration as e:
    print(e.value)

print("-- awaitables of one's own")


class Ready:
    def __init__(self, v):
        self.v = v

    def __await__(self):
        return iter([])


class Twice:
    def __await__(self):
        a = yield "one"
        b = yield "two"
        return a, b


async def use():
    print("  ready", await Ready(1))
    return await Twice()


c = use()
print(c.send(None), c.send("A"))
try:
    c.send("B")
except StopIteration as e:
    print("returned", e.value)

print("-- refusals")


class NotIter:
    def __await__(self):
        return 5


class GivesCoro:
    def __await__(self):
        return self.c

    def __init__(self):
        self.c = leaf(0)


async def await_int():
    await 3


async def await_notiter():
    await NotIter()


g = GivesCoro()


async def await_coro():
    await g


async def await_agen():
    async def ag():
        yield
    a = ag()
    try:
        await a
    finally:
        await a.aclose()


for f in (await_int, await_notiter, await_coro, await_agen):
    drive(f())
g.c.close()

parked = chain(0)
parked.send(None)


async def await_parked():
    await parked


drive(await_parked())
parked.close()


async def await_self():
    await me


me = await_self()
drive(me)

x = leaf(0)
try:
    iter(x)
except TypeError as e:
    print(e)
x.close()


def plain(c):
    yield from c


x = leaf(0)
try:
    next(plain(x))
except TypeError as e:
    print(e)
x.close()


@iterable
def delegating():
    return (yield from chain(0))


d = delegating()
print(next(d), end=" ")
try:
    d.send("through")
except StopIteration as e:
    print(e.value)

print("-- PEP 479")


async def stops():
    raise StopIteration("inner")


drive(stops())

print("-- what the flags and the types say")


async def af():
    pass


async def ag():
    yield


def gf():
    yield


print(hex(af.__code__.co_flags), hex(ag.__code__.co_flags), hex(gf.__code__.co_flags))
print(hex(tick.__code__.co_flags))
x, y = af(), ag()
print(type(x) is _types.CoroutineType, type(y) is _types.AsyncGeneratorType)
print(repr(x).startswith("<coroutine object af at 0x"), repr(y).startswith("<async_generator object ag at 0x"))
x.close()
