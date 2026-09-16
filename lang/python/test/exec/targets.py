# Parenthesized with items, star targets in for, and del with a trailing comma.
class M:
    def __init__(self, n): self.n = n
    def __enter__(self): print("enter", self.n); return self.n
    def __exit__(self, *a): print("exit", self.n)
del_me = z = a = 1
del (z), [a],
for a, *b in [(1, 2, 3)]: print(a, b)
for *b, a in [(1, 2, 3)]: print(a, b)
with (M(1) as f, M(2)): print(f)
with (M(3) as f, M(4) as g,): print(f, g)
with (M(5)) as f: print(f)
with (M(6)): print("six")
with (M(7), M(8)): print("pair")
with (M(9) if a else M(0)) as h, M(10): print(h)
print(*[] or [1], *(x for x in [2] if x))


# Starred items in yield, augmented assignment and f-string lists.
def g():
    rest = 4, 5
    yield 1, *rest
    x = [0]
    x += *rest, 6
    yield x
    yield f"{*rest,}"
print(list(g()))
