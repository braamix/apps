def f():
    yield
    yield 1
    yield from g()
    x = yield 2

def h():
    return (yield)
