async def fetch(src):
    return await src ** 2


async def walk(it, cm):
    async with cm as c:
        async for x in it:
            if x:
                break
        else:
            return c
    return [y async for y in it if await y]


async def ticks(n):
    for i in range(n):
        yield i
    yield await fetch(n)
