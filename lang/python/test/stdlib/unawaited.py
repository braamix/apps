# A coroutine collected without having started says so through warnings.
import gc
import warnings


async def f():
    return 1


with warnings.catch_warnings(record=True) as log:
    warnings.simplefilter("always")
    c = f()
    del c
    gc.collect()
print([(w.category.__name__, str(w.message)) for w in log])
