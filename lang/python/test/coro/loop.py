# A small event loop in Python, which is what asyncio is at bottom: tasks are
# coroutines, a sleep is a value yielded up through every await to the loop,
# and the loop decides who runs next by a clock of its own.


def iterable(f):
    f.__code__ = f.__code__.replace(co_flags=f.__code__.co_flags | 0x100)
    return f


@iterable
def sleep(ticks):
    return (yield ("sleep", ticks))


@iterable
def spawn(coro):
    return (yield ("spawn", coro))


@iterable
def join(task):
    return (yield ("join", task))


class Task:
    count = 0

    def __init__(self, coro):
        Task.count += 1
        self.id = Task.count
        self.coro = coro
        self.done = False
        self.result = None
        self.error = None
        self.waiters = []


class Loop:
    def __init__(self):
        self.now = 0
        self.ready = []  # (when, order, task, value)
        self.order = 0

    def schedule(self, task, when, value=None):
        self.order += 1
        self.ready.append((when, self.order, task, value))

    def run(self, coro):
        main = Task(coro)
        self.schedule(main, 0)
        steps = 0
        while self.ready:
            self.ready.sort(key=lambda r: (r[0], r[1]))
            when, _, task, value = self.ready.pop(0)
            self.now = when
            steps += 1
            try:
                op, arg = task.coro.send(value)
            except StopIteration as e:
                self.finish(task, e.value, None)
                continue
            except Exception as e:
                self.finish(task, None, e)
                continue
            if op == "sleep":
                self.schedule(task, self.now + arg, self.now)
            elif op == "spawn":
                child = Task(arg)
                self.schedule(child, self.now)
                self.schedule(task, self.now, child)
            elif op == "join":
                if arg.done:
                    self.schedule(task, self.now, arg)
                else:
                    arg.waiters.append(task)
        return main, steps

    def finish(self, task, result, error):
        task.done = True
        task.result = result
        task.error = error
        for w in task.waiters:
            self.schedule(w, self.now, task)


log = []


async def worker(name, delays):
    for d in delays:
        await sleep(d)
        log.append((loop.now, name))
    return name.upper()


async def failing():
    await sleep(3)
    raise ValueError("worker failed")


async def gather(*coros):
    tasks = [await spawn(c) for c in coros]
    out = []
    for t in tasks:
        t = await join(t)
        out.append(t.error if t.error else t.result)
    return out


async def deep(n):
    if n == 0:
        total = 0
        for i in range(50):
            total += await sleep(1)
        return total
    return await deep(n - 1)


async def ticker(n):
    for i in range(n):
        await sleep(2)
        yield i


async def consumer():
    got = []
    async for v in ticker(4):
        got.append((loop.now, v))
    return got


async def main():
    results = await gather(worker("a", [1, 4, 1]), worker("b", [2, 2, 2]),
                           failing(), deep(120), consumer())
    return [type(r).__name__ + ": " + str(r) if isinstance(r, Exception) else r
            for r in results]


loop = Loop()
task, steps = loop.run(main())
print(task.result)
print(log)
print("clock", loop.now, "steps", steps, "tasks", Task.count)


async def many():
    tasks = [await spawn(worker("w%d" % i, [i % 7 + 1] * 3)) for i in range(60)]
    names = []
    for t in tasks:
        names.append((await join(t)).result)
    return len(names), names[:3], names[-1]


log.clear()
loop = Loop()
task, steps = loop.run(many())
print(task.result, len(log), log[:4], "clock", loop.now, "steps", steps)
