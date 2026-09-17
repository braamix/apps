import asyncio
import sys

async def work(n, out):
    await asyncio.sleep(0.01 * n)
    out.append(n)
    return n * 2

async def main():
    out = []
    got = await asyncio.gather(work(3, out), work(1, out), work(2, out))
    print("gather", got, out)

    t = asyncio.create_task(work(0, out))
    print("task", await t, t.done(), t.get_name().startswith("Task-"))

    q = asyncio.Queue()
    await q.put(1)
    await q.put(2)
    print("queue", q.qsize(), await q.get(), await q.get())

    lock = asyncio.Lock()
    async with lock:
        print("lock", lock.locked())

    ev = asyncio.Event()
    async def setter():
        await asyncio.sleep(0)
        ev.set()
    asyncio.create_task(setter())
    await ev.wait()
    print("event", ev.is_set())

    async with asyncio.TaskGroup() as tg:
        a = tg.create_task(work(1, out))
        b = tg.create_task(work(2, out))
    print("group", a.result(), b.result())

    try:
        async with asyncio.timeout(0.01):
            await asyncio.sleep(1)
    except TimeoutError:
        print("timeout fired")

    done, pending = await asyncio.wait([asyncio.create_task(work(1, out))])
    print("wait", len(done), len(pending))

    print("shield", await asyncio.shield(work(0, out)))
    print("loop", asyncio.get_running_loop() is asyncio.get_event_loop())

    async def cancelme():
        try:
            await asyncio.sleep(10)
        except asyncio.CancelledError:
            print("cancelled")
            raise
    c = asyncio.create_task(cancelme())
    await asyncio.sleep(0)
    c.cancel()
    try:
        await c
    except asyncio.CancelledError:
        print("propagated")

asyncio.run(main())
print("done")


# PEP 525: the loop is told when an async generator starts, so the one this
# leaves half-read is closed when the loop shuts down.
async def nums():
    try:
        for i in range(3):
            yield i
    finally:
        print("agen closed")


# The generator is held, so it is the loop's shutdown that closes it and not
# whenever the collector happens to run.
held = None


async def agens():
    global held
    held = nums()
    async for n in held:
        print("agen", n)
        if n == 1:
            break
    print("hooks set:", sys.get_asyncgen_hooks() != (None, None))


asyncio.run(agens())

# The loop itself, outside asyncio.run().
loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)
order = []
loop.call_soon(order.append, "soon")
loop.call_later(0.01, order.append, "later")
h = loop.call_later(0.01, order.append, "cancelled")
h.cancel()
loop.call_later(0.02, loop.stop)
loop.run_forever()
print("order", order, h.cancelled())
print("closed", loop.is_closed(), loop.is_running())
loop.close()
print("closed", loop.is_closed())
try:
    loop.run_forever()
except RuntimeError as e:
    print("RuntimeError", e)
