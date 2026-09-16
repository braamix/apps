# operator, and the three callable objects the library reaches for. An
# operand that is a class instance is the interesting case: its operator is
# Python, and a builtin cannot call it, so `add` is a state machine rather
# than one dispatch.
import operator as op


class Money:
    def __init__(self, n):
        self.n = n

    def __add__(self, other):
        return Money(self.n + other.n)

    def __radd__(self, other):
        return Money(self.n + other)

    def __lt__(self, other):
        return self.n < other.n

    def __neg__(self):
        return Money(-self.n)

    def __repr__(self):
        return "Money(%d)" % self.n


print(op.add(2, 3), op.sub(5, 2), op.mul(3, 4), op.truediv(7, 2), op.floordiv(7, 2))
print(op.mod(7, 3), op.pow(2, 10), op.and_(12, 10), op.or_(12, 10), op.xor(12, 10))
print(op.lshift(1, 10), op.rshift(1024, 3), op.neg(5), op.pos(-5), op.invert(5))
print(op.abs(-5), op.abs(-5.5), op.abs(3 + 4j), op.not_(0), op.truth([]))
print(op.add("a", "b"), op.mul("ab", 2), op.mod("%d", 7), op.concat([1], [2]))

print(op.eq(1, 1), op.ne(1, 2), op.lt(1, 2), op.le(2, 2), op.gt(3, 2), op.ge(2, 3))
print(op.is_(None, None), op.is_not(1, 2), op.index(7), op.length_hint([1, 2]))
print(op.length_hint("abc", 9), op.contains([1, 2], 2), op.countOf([1, 2, 1], 1))
print(op.indexOf([1, 2, 1], 2))

d = {"a": 1}
op.setitem(d, "b", 2)
print(op.getitem(d, "b"), d)
op.delitem(d, "a")
print(d)

xs = [1, 2]
print(op.iadd(xs, [3]), xs)
print(op.iconcat([1], [2]), op.imul([1], 3))

print(op.add(Money(1), Money(2)), op.add(3, Money(4)), op.neg(Money(5)))
print(op.lt(Money(1), Money(2)), op.gt(Money(1), Money(2)), op.eq(Money(1), Money(1)))
print(sorted([Money(3), Money(1), Money(2)]))

print(op.itemgetter(1)([9, 8, 7]), op.itemgetter(0, 2)("abc"))
print(op.itemgetter("a")({"a": 5}), op.itemgetter(slice(1, 3))([1, 2, 3, 4]))
print(op.attrgetter("n")(Money(7)), op.attrgetter("n", "n")(Money(7)))
class Wrap:
    def __init__(self, m):
        self.m = m


print(op.attrgetter("real")(3 + 4j), op.attrgetter("m.n")(Wrap(Money(7))))
print(op.methodcaller("upper")("hi"), op.methodcaller("replace", "a", "b")("cat"))
print(op.methodcaller("count", 1)([1, 1, 2]))
print(sorted([(2, "b"), (1, "a")], key=op.itemgetter(0)))
print(sorted([Money(3), Money(1)], key=op.attrgetter("n")))
print(list(map(op.methodcaller("strip"), [" a ", " b"])))

for call in ("op.add(1, 'a')", "op.itemgetter()", "op.attrgetter(1)",
             "op.index(1.5)", "op.getitem([1], 9)"):
    try:
        eval(call)
        print(call, "no error")
    except Exception as e:
        print(call, type(e).__name__)
