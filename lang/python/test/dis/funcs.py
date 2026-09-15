def plain(a, b):
    return a + b

def defaults(a, b=1, *rest, c, d=2, **kw):
    return a, b, rest, c, d, kw

def positional(a, b, /, c):
    return a + b + c

def deco(f):
    return f

@deco
@deco
def wrapped():
    pass

f = lambda x, y=2: x * y
plain(1, b=2)
plain(*d, **kw)
plain(*d, b=1, **kw)
