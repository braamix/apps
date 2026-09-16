# Async generators: asend, athrow and aclose, what their awaitables do when
# stepped by hand, and aiter() and anext().


def iterable(f):
    f.__code__ = f.__code__.replace(co_flags=f.__code__.co_flags | 0x100)
    return f


@iterable
def pause(tag):
    got = yield tag
    return got


def step(aw, *sends):
    out = []
    try:
        out.append(aw.send(None))
        for v in sends:
            out.append(aw.send(v))
        print("  still running", out)
    except StopIteration as e:
        print("  ", out, "stop", e.value)
    except BaseException as e:
        print("  ", out, "raised", type(e).__name__, e)


def run(c):
    out = []
    try:
        while True:
            out.append(c.send(len(out) or None))
    except StopIteration as e:
        print("  pauses", out, "->", e.value)
    except BaseException as e:
        print("  pauses", out, "raised", type(e).__name__, e)


async def counter(n):
    try:
        for i in range(n):
            got = yield i
            if got is not None:
                print("  got", got)
            await pause("after %d" % i)
    finally:
        print("  counter closed")


print("-- stepping the awaitables by hand")
g = counter(3)
print(type(g).__name__, g.__name__, g.__qualname__, g.ag_running, g.ag_await)
h = counter(0)
a1, a2 = h.__anext__(), h.aclose()
print(g.__aiter__() is g, type(a1).__name__, type(a2).__name__)
print(a1.close(), a2.close())
step(g.__anext__())
step(g.asend("hello"), "resumed")
print(g.ag_running, g.ag_frame is not None, g.ag_code.co_name)
aw = g.__anext__()
step(aw)
step(aw)
step(g.aclose())
print(g.ag_frame, g.ag_running)
step(g.__anext__())
step(g.aclose())
step(g.athrow(ValueError("late")))

print("-- athrow")


async def catcher():
    while True:
        try:
            yield "ready"
        except KeyError as e:
            print("  caught", repr(e))
            yield "handled"


g = catcher()
step(g.__anext__())
step(g.athrow(KeyError("one")))
step(g.athrow(KeyError("two")))
step(g.athrow(IndexError("fatal")))
step(g.__anext__())

g = catcher()
step(g.athrow(ValueError("unstarted")))
step(g.__anext__())

print("-- aclose")


async def refuses():
    try:
        yield 1
    except GeneratorExit:
        yield 2


g = refuses()
step(g.__anext__())
step(g.aclose())


async def awaits_in_finally():
    try:
        yield 1
    finally:
        print("  cleaning up")
        await pause("cleanup")
        print("  cleaned")


g = awaits_in_finally()
step(g.__anext__())
c = g.aclose()
print(c.send(None), g.ag_running, type(g.ag_await).__name__)
try:
    c.send(None)
except StopIteration:
    print("  closed", g.ag_running)

g = counter(1)
step(g.aclose())
step(g.__anext__())

print("-- the ends of an async generator")


async def returns():
    yield 1
    return


async def raises_sai():
    yield 1
    raise StopAsyncIteration


async def raises_si():
    yield 1
    raise StopIteration


for f in (returns, raises_sai, raises_si):
    g = f()
    step(g.__anext__())
    step(g.__anext__())
    step(g.__anext__())

print("-- reusing and overlapping")


async def slow():
    await pause("slow")
    yield "value"


g = slow()
first = g.__anext__()
print(first.send(None))
step(g.__anext__())
step(g.aclose())
step(g.athrow(ValueError))
step(first, None)
step(first)
try:
    first.throw(ValueError("again"))
except RuntimeError as e:
    print(e)
print(first.close())
step(g.aclose())

g = catcher()
a = g.aclose()
try:
    a.send("x")
except RuntimeError as e:
    print(e)
step(a)

print("-- close() and throw() on the awaitables")
g = counter(5)
step(g.__anext__())
aw = g.__anext__()
print(aw.send(None))
print(aw.close())
step(g.__anext__())

g = counter(5)
step(g.__anext__())
aw = g.__anext__()
try:
    aw.throw(KeyError("thrown"))
except KeyError as e:
    print("  thrown back", e)
step(aw)
step(g.__anext__())

print("-- async for over one, and the builtins")


async def consume():
    out = [x async for x in counter(3)]
    it = aiter(counter(2))
    out.append(await anext(it))
    out.append(await anext(it, "d1"))
    out.append(await anext(it, "d2"))
    try:
        await anext(it)
    except StopAsyncIteration:
        out.append("done")
    return out


run(consume())


class Mine:
    def __init__(self):
        self.n = 2

    def __aiter__(self):
        return self

    async def __anext__(self):
        if not self.n:
            raise StopAsyncIteration
        self.n -= 1
        return await pause(self.n)


async def builtins():
    m = aiter(Mine())
    return [await anext(m), await anext(m, "x"), await anext(m, "default")]


run(builtins())
x = anext(counter(1), 0)
print(type(x).__name__, x.close())

for bad in (lambda: aiter(1), lambda: anext(1)):
    try:
        bad()
    except TypeError as e:
        print(e)


class BadAiter:
    def __aiter__(self):
        return 3


try:
    aiter(BadAiter())
except TypeError as e:
    print(e)

print("-- an async generator expression")


async def gexp():
    squares = (x * x async for x in counter(3) if x)
    return [v async for v in squares]


run(gexp())
