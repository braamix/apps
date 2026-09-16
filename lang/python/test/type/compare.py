# Sorting and searching a sequence whose items compare in Python.
#
# Each comparison here is a call the sort itself has to make, so the loops are
# continuations rather than plain C++ ones. The same program on CPython and
# here, byte for byte, which is what says the two merges agree.


class Box:
    def __init__(self, v, tag=''):
        self.v = v
        self.tag = tag

    def __lt__(self, other):
        return self.v < other.v

    def __eq__(self, other):
        return isinstance(other, Box) and self.v == other.v

    def __repr__(self):
        return 'Box(%d%s)' % (self.v, self.tag)

    __hash__ = None


xs = [Box(3, 'a'), Box(1, 'b'), Box(2, 'c'), Box(1, 'd'), Box(3, 'e')]
print(sorted(xs))
print(sorted(xs, reverse=True))
print(min(xs), max(xs))
print(sorted(xs, key=lambda b: b))
print(min(xs, key=lambda b: b), max(xs, key=lambda b: b))

# Stable: two equal keys keep the order they were written in, and reversing
# does not reverse a tie.
print([b.tag for b in sorted(xs)])
print([b.tag for b in sorted(xs, reverse=True)])

ys = list(xs)
ys.sort()
print(ys)
ys.sort(reverse=True)
print(ys)
ys.sort(key=lambda b: -b.v)
print(ys)

# The searches, which ask __eq__ rather than __lt__.
print(xs.index(Box(2)), xs.count(Box(1)), xs.count(Box(9)))
print(Box(3) in xs, Box(9) in xs, Box(9) not in xs)
print(tuple(xs).index(Box(2)), tuple(xs).count(Box(3)))

zs = list(xs)
zs.remove(Box(3))
print(zs)
try:
    zs.remove(Box(99))
except ValueError:
    print('ValueError')

try:
    xs.index(Box(99))
except ValueError:
    print('ValueError')


# A class with only __lt__ still answers `>`, because the reflected call is
# tried even between two of the same class.
class Only:
    def __init__(self, v):
        self.v = v

    def __lt__(self, other):
        return self.v < other.v

    def __repr__(self):
        return 'Only(%d)' % self.v


os_ = [Only(2), Only(1), Only(3)]
print(sorted(os_), max(os_), min(os_))
print(Only(1) < Only(2), Only(3) > Only(1))


# NotImplemented on both sides, for a comparison that cannot be made.
class Never:
    def __lt__(self, other):
        return NotImplemented

    def __eq__(self, other):
        return NotImplemented

    __hash__ = None


try:
    sorted([Never(), Never()])
except TypeError:
    print('TypeError')

n = Never()
print(n == n, [n].index(n))


# A long one, so the merge really does pass more than once and the comparison
# count is what a merge sort's is rather than a scan's.
class Counted:
    calls = 0

    def __init__(self, v):
        self.v = v

    def __lt__(self, other):
        Counted.calls += 1
        return self.v < other.v


big = [Counted((i * 37) % 64) for i in range(64)]
out = sorted(big)
print([b.v for b in out] == sorted(b.v for b in big))
print(Counted.calls > 64, Counted.calls < 64 * 64)


# An empty list and a single item need no comparison at all.
print(sorted([]), sorted([Box(1)]))
print([] == [], [Box(1)] == [Box(1)])


# Nested: a list of lists of things that compare in Python, and a tuple beside
# a list, which are never equal however their items compare.
print([[Box(1)], [Box(2)]] == [[Box(1)], [Box(2)]])
print([[Box(1)]] == [[Box(2)]])
print(((Box(1),), (Box(2),)) == ((Box(1),), (Box(2),)))
print([Box(1)] != [Box(1)], [Box(1)] != [Box(2)])
print([Box(1)] == (Box(1),))
print([Box(1), Box(2)] == [Box(1)])
