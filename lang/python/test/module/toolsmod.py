# _functools: reduce, partial and the lru_cache wrapper. All three call a
# function the program wrote, so all three are continuations rather than
# loops -- what is checked here is that the answers come out in order.
from _functools import reduce, partial, _lru_cache_wrapper

print(reduce(lambda a, b: a + b, [1, 2, 3, 4]))
print(reduce(lambda a, b: a + b, [1, 2, 3, 4], 10))
print(reduce(lambda a, b: a * b, range(1, 6)), reduce(lambda a, b: a + b, [], 0))
print(reduce(lambda a, b: a + b, "abc"), reduce(max, [3, 1, 4, 1, 5]))
try:
    reduce(lambda a, b: a, [])
except TypeError as e:
    print("TypeError")

p = partial(pow, 2)
print(p(10), p.func, p.args, p.keywords)


def f(a, b=0, c=0):
    return (a, b, c)


q = partial(f, 1, c=3)
print(q(2), q(2, c=9))
print(partial(f, c=1)(9), partial(f)(1, 2, 3))
print(sorted(map(partial(pow, 2), [1, 2, 3])))
r = partial(f, 1)
r.note = "kept"
print(r.note, r.__dict__)

# CPython's own wrapper takes the type its cache_info() is to build with, so
# the test hands both interpreters one that answers a plain tuple.
def info_type(hits, misses, maxsize, currsize):
    return (hits, misses, maxsize, currsize)


calls = []


def slow(n):
    calls.append(n)
    return n * n


w = _lru_cache_wrapper(slow, 2, False, info_type)
print(w(2), w(3), w(2), calls)
print(w(4), calls)
print(w(3), calls)
print(w.cache_info())
print(w.cache_info()[0], w.cache_info()[3], tuple(w.cache_info()))
w.cache_clear()
print(w.cache_info())

seen = []


def kw(a, b=1):
    seen.append((a, b))
    return a + b


c = _lru_cache_wrapper(kw, None, False, info_type)
print(c(1), c(1), c(1, b=2), c(1, b=2), seen)
print(c.cache_info())


def fib(n):
    return n if n < 2 else fib(n - 1) + fib(n - 2)


fib = _lru_cache_wrapper(fib, None, False, info_type)
print(fib(30), fib.cache_info()[1])

t = _lru_cache_wrapper(lambda x: x, 128, True, info_type)
print(t(1), t(1.0), t.cache_info()[1])
z = _lru_cache_wrapper(slow, 0, False, info_type)
calls.clear()
print(z(5), z(5), calls, z.cache_info())
