def f():
    x = 1
    del x
    class C:
        y = x
        global g
        g = 1
    return C

def r():
    try:
        return 1
    finally:
        pass

def w():
    with open('a'):
        return 2

def gen():
    yield 1
    x = yield
    return 3
