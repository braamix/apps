# async for, async with and the async comprehensions: where each awaits, and
# how each is left.


def iterable(f):
    f.__code__ = f.__code__.replace(co_flags=f.__code__.co_flags | 0x100)
    return f


@iterable
def pause(tag):
    got = yield tag
    return got


def run(c):
    out = []
    try:
        while True:
            out.append(c.send(len(out) or None))
    except StopIteration as e:
        print("  pauses", out, "->", e.value)
    except BaseException as e:
        print("  pauses", out, "raised", type(e).__name__, e)


class Countdown:
    def __init__(self, n, wait=False):
        self.n = n
        self.wait = wait

    def __aiter__(self):
        return self

    async def __anext__(self):
        if self.wait:
            await pause("next")
        if self.n == 0:
            raise StopAsyncIteration
        self.n -= 1
        return self.n


class Wrapper:
    def __init__(self, n):
        self.n = n

    def __aiter__(self):
        return Countdown(self.n)


class Manager:
    def __init__(self, name, swallow=False, wait=False):
        self.name = name
        self.swallow = swallow
        self.wait = wait

    async def __aenter__(self):
        print("  enter", self.name)
        if self.wait:
            await pause("enter " + self.name)
        return self.name.upper()

    async def __aexit__(self, t, e, tb):
        print("  exit", self.name, t.__name__ if t else None, e)
        if self.wait:
            await pause("exit " + self.name)
        return self.swallow


print("-- async for, with its else")


async def loop(it):
    total = []
    async for x in it:
        total.append(x)
    else:
        total.append("else")
    return total


run(loop(Countdown(3)))
run(loop(Wrapper(2)))
run(loop(Countdown(2, wait=True)))

print("-- break, continue and return leave the right way")


async def leave(it):
    seen = []
    async for x in it:
        if x == 3:
            continue
        if x == 1:
            break
        seen.append(x)
    else:
        seen.append("no else")
    async for x in it:
        return seen, x


run(leave(Countdown(5)))


async def nested():
    out = []
    async for a in Countdown(2):
        async for b in Countdown(3, wait=True):
            if b == 1:
                break
            out.append((a, b))
    return out


run(nested())

print("-- an exception out of the body is not the end of the loop")


async def raising():
    try:
        async for x in Countdown(3):
            raise KeyError(x)
    except KeyError as e:
        return "caught " + repr(e)


run(raising())


class Broken:
    def __aiter__(self):
        return self

    async def __anext__(self):
        raise ValueError("broken")


run(loop(Broken()))

print("-- async with")


async def body(m, fail=False):
    async with m as got:
        print("  body", got)
        if fail:
            raise LookupError("in body")
        await pause("body")
    return "after"


run(body(Manager("a")))
run(body(Manager("b", wait=True)))
run(body(Manager("c"), fail=True))
run(body(Manager("d", swallow=True), fail=True))


async def several():
    async with Manager("x") as x, Manager("y", wait=True) as y:
        return x + y


run(several())


async def escapes():
    for i in range(3):
        async with Manager("loop%d" % i):
            if i == 0:
                continue
            if i == 1:
                break
    async with Manager("ret", wait=True):
        try:
            return "returned"
        finally:
            print("  finally")


run(escapes())

print("-- the protocol refused")


class OnlyEnter:
    async def __aenter__(self):
        pass


class BadEnter:
    def __aenter__(self):
        return 1

    async def __aexit__(self, *a):
        pass


class BadExit:
    async def __aenter__(self):
        pass

    def __aexit__(self, *a):
        return 2


class NoANext:
    def __aiter__(self):
        return 7


class BadANext:
    def __aiter__(self):
        return self

    def __anext__(self):
        return 8


async def with_it(m):
    async with m:
        pass


async def for_it(it):
    async for x in it:
        pass


for m in (1, OnlyEnter(), BadEnter(), BadExit()):
    run(with_it(m))
for it in (1, NoANext(), BadANext()):
    run(for_it(it))

print("-- comprehensions")


async def comps():
    a = [x async for x in Countdown(4)]
    b = {x % 2 async for x in Countdown(4, wait=True)}
    c = {x: await pause(x) async for x in Countdown(2)}
    d = [y for y in range(4) if await pause(y) % 2]
    e = [[z async for z in Countdown(x)] for x in range(3)]
    f = [x async for x in Countdown(6) if x % 2 async for y in Countdown(x) if y == 0]
    return a, sorted(b), c, d, e, f


run(comps())


async def genexp():
    g = (x * 10 async for x in Countdown(3))
    h = (await pause(x) for x in "ab")
    print("  ", type(g).__name__, type(h).__name__)
    return [v async for v in g], [v async for v in h]


run(genexp())
