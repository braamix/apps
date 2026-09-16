# Where `await`, `async for`, `async with` and the async comprehensions may
# be written, and what each place compiles to.


class Both:
    def __iter__(self):
        return iter(())

    def __aiter__(self):
        return self

    async def __anext__(self):
        raise StopAsyncIteration


def check(src):
    try:
        code = compile(src, "<case>", "exec")
    except SyntaxError as e:
        print("refused ", repr(src), "--", e.args[0])
        return
    ns = {"y": Both()}
    exec(code, ns)
    f = ns.get("f")
    kind = "-"
    if f is not None:
        flags = f.__code__.co_flags
        kind = "coroutine" if flags & 0x80 else "async generator" if flags & 0x200 \
            else "generator" if flags & 0x20 else "function"
    print("compiled", repr(src), "--", kind)


for src in [
    "await x",
    "class C: await x",
    "def f(): await x",
    "lambda: await x",
    "f = lambda: (await x for x in y)",
    "async def f(): await x",
    "async def f():\n def g(): await x",
    "async def f(): return lambda: await x",
    "async def f(): return [await z for z in y]",
    "async def f(): return [[z async for z in x] for x in y]",
    "async def f(): return {k: v async for k, v in y}",
    "async def f(): return (z async for z in y)",
    "async for x in y: pass",
    "async with x: pass",
    "def f():\n async for x in y: pass",
    "def f():\n async with x: pass",
    "async def f():\n async with a as b, c:\n  async for d in b: pass",
    "def f(): return [x async for x in y]",
    "def f(): return [await x for x in y]",
    "def f(): return (x async for x in y)",
    "def f(): return (await x for x in y)",
    "def f(): return [[z async for z in x] for x in y]",
    "[x async for x in y]",
    "(x async for x in y)",
    "async def f(): yield from x",
    "async def f():\n yield 1\n return 2",
    "async def f():\n return 2\n yield 1",
    "async def f():\n yield 1\n return",
    "async def f(): await (yield)",
    "async def f(): x = yield",
    "async def f(): pass",
    "def f(): yield",
    "async def f(): return await x ** 2",
    "async def f(): return -await x",
    "async def f(): return await await x",
]:
    check(src)
