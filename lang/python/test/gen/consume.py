# Everything that takes an iterable, handed a generator. Each of these is a
# builtin that cannot step one itself, so each parks and is run again over a
# list; see iter_park in call.h.

def g(n):
    for i in range(n):
        yield i


def pairs(n):
    for i in range(n):
        yield chr(ord("a") + i), i


print(list(g(3)))
print(tuple(g(3)))
print(set(g(3)))
print(frozenset(g(3)))
print(dict(pairs(3)))
print(sorted(g(3), reverse=True))
print(sorted(g(4), key=lambda v: -v))
print(min(g(3)), max(g(3)))
print(min(g(3), key=lambda v: -v), max(g(3), default="none"))
print(min(g(0), default="none"))
print(sum(g(4)), sum(g(4), 10))
print(any(g(3)), all(g(3)), any(g(0)), all(g(0)))
print(list(enumerate(g(3))))
print(list(enumerate(g(3), 5)))
print(list(zip(g(3), "abc")))
print(list(zip("abc", g(3))))
print(list(map(lambda v: v + 1, g(3))))
print(list(map(lambda a, b: (a, b), g(3), g(3))))
print(list(filter(None, g(3))))
print(bytes(g(3)))
print(bytearray(g(3)))
print(int.from_bytes(g(2), "big"))
print("-".join(str(x) for x in g(3)))
print(b"-".join(bytes([x]) for x in g(3)))
print(dict.fromkeys(g(3)))
print(next(g(3)), next(g(0), "none"))

xs = [9]
xs.extend(g(3))
print(xs)
xs += g(2)
print(xs)
ba = bytearray(b"z")
ba.extend(g(2))
print(ba)
d = {"z": 9}
d.update(pairs(2))
print(d)

# The opcodes that iterate, rather than the builtins.
print([*g(3)])
print((*g(3),))
print({*g(3)})
print({**dict(pairs(2))})
a, b, c = g(3)
print(a, b, c)
head, *tail = g(4)
print(head, tail)
*most, last = g(4)
print(most, last)


def three(p, q, r):
    return p * 100 + q * 10 + r


print(three(*g(3)))
print(1 in g(3), 9 in g(3), 9 not in g(3))

# A generator of generators, drained twice over.
print([list(x) for x in (g(i) for i in range(4))])

# An empty one everywhere.
print(list(g(0)), tuple(g(0)), set(g(0)), sum(g(0)), "".join(str(x) for x in g(0)))


# A class that writes its own __iter__ and __next__ is the same problem as a
# generator, and parks the same way.
class Counter:
    def __init__(self, n):
        self.n = n
        self.i = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.i >= self.n:
            raise StopIteration
        self.i += 1
        return self.i


print(list(Counter(3)))
print(sum(Counter(4)))
print(sorted(Counter(3), reverse=True))
print("".join(str(x) for x in Counter(3)))
print([*Counter(2)], {*Counter(2)}, tuple(Counter(2)))
p, q = Counter(2)
print(p, q)
print(2 in Counter(3))
print(next(Counter(2)))


# A class whose __iter__ hands back something else.
class Wrapper:
    def __init__(self, xs):
        self.xs = xs

    def __iter__(self):
        return iter(self.xs)


print(list(Wrapper([1, 2, 3])), sum(Wrapper([1, 2])), max(Wrapper("abc")))


class Gened:
    def __iter__(self):
        yield 1
        yield 2


print(list(Gened()), sum(Gened()))
