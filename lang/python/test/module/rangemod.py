# range past a small integer: the two iterators, slicing, reversal and the
# search methods, with CPython's rule for which iterator a range gets.
r = range(1 << 1000)
print(type(iter(r)).__name__, r[5], r[-1] == (1 << 1000) - 1, bool(r), (1 << 999) in r, r.index(7), r.count(3))
print(repr(range(-(1 << 70), 1 << 70, 3)), r.start, r.stop == 1 << 1000, r.step)
try:
    len(r)
except OverflowError as e:
    print(e)
print(hash(range(0)) == hash(range(5, 5)), range(0, 10, 3) == range(0, 11, 3), range(1, 2, 5) == range(1, 3, 7))
print(type(reversed(r)).__name__, next(reversed(r)) == (1 << 1000) - 1, list(reversed(range(1, 10, 3))))
print(r[10:20], r[::-(1 << 500)][:2] == range(r[-1], r[-1] - 2 * (1 << 500), -(1 << 500)), range(10)[1 << 100:])
print(list(range(3, -3, -2)), list(range(10)[::-3]), list(range(10)[-3:2:-1]), range(10)[2:-2:2])
print([type(iter(range(n))).__name__ for n in (2**62, 2**63, 2**64)], type(iter(range(-2**63, 0))).__name__)
it = iter(range((1 << 64) - 2, (1 << 64) + 2))
print(list(it), list(range(2**63 - 3, 2**63 + 1)), list(range(-2**63 + 1, -2**63 - 2, -1)))
for bad in [lambda: range(10)[1 << 100], lambda: range(1.5), lambda: range(1, 2, 0), lambda: range(10)["x"],
            lambda: range(3).index(7), lambda: range(3).index("x"), lambda: range(3)[1:2:0]]:
    try:
        bad()
    except (IndexError, TypeError, ValueError) as e:
        print(type(e).__name__, e)
print(3.0 in range(5), range(10).index(True), range(10).count(1.0), "x" in range(3), range(3).count("x"))
print(range(True, 5)[0], range(4)[True], len(range(0, -5)), len(range(5, 0, -2)), list(range(5, 0, -2)))
print(sorted({range(3), range(0, 3), range(0, 3, 1)}, key=len), range(3) != range(4), range(2) == [0, 1])
print(range(10)[2:8:3], range(0, 100, 7)[-3:], sum(range(10**6)), max(range(-5, 5)))
