# itertools. The lazy half is checked through islice, which is the only way to
# look at an infinite iterator; the eager half is checked for what it yields,
# because when it yields is this port's own business -- README says why.
from itertools import (count, cycle, repeat, chain, compress, islice, starmap,
                       takewhile, dropwhile, filterfalse, accumulate, groupby,
                       tee, zip_longest, product, permutations, combinations,
                       combinations_with_replacement)

print(list(islice(count(), 4)), list(islice(count(10, 5), 4)))
print(list(islice(count(2.5, 0.5), 3)), list(islice(count(10 ** 19, 1), 2)))
print(list(islice(cycle("abc"), 7)), list(cycle([])))
print(list(repeat("x", 3)), list(islice(repeat(0), 4)), list(repeat(1, 0)))

print(list(chain([1, 2], [3], [], [4, 5])), list(chain()))
print(list(chain.from_iterable([[1, 2], [3]])))
print(list(compress("abcdef", [1, 0, 1, 0, 1, 1])), list(compress("ab", [])))

print(list(islice("abcdefg", 3)), list(islice("abcdefg", 2, 6)))
print(list(islice("abcdefg", 2, 6, 2)), list(islice("abcdefg", 0, None, 3)))
print(list(islice(count(), 2, 8, 3)))

print(list(zip_longest([1, 2, 3], "ab")), list(zip_longest("ab", [1], fillvalue="-")))
print(list(zip_longest()), list(zip_longest([], [])))

print(list(product([1, 2], "ab")), list(product()))
print(list(product("ab", repeat=2)), list(product([], "ab")))
print(list(product("ab", repeat=0)))
print(list(permutations("abc")), list(permutations("abc", 2)))
print(list(permutations("ab", 3)), list(permutations([], 0)))
print(list(combinations("abcd", 2)), list(combinations("ab", 3)))
print(list(combinations_with_replacement("ab", 2)))
print(list(combinations_with_replacement("ab", 0)))

print(list(takewhile(lambda x: x < 3, [1, 2, 3, 1])))
print(list(dropwhile(lambda x: x < 3, [1, 2, 3, 1])))
print(list(filterfalse(lambda x: x % 2, range(6))), list(filterfalse(None, [0, 1, [], "a"])))
print(list(starmap(pow, [(2, 3), (3, 2)])), list(starmap(lambda *a: sum(a), [(1, 2, 3)])))
print(list(accumulate([1, 2, 3, 4])), list(accumulate([])))
print(list(accumulate([1, 2, 3, 4], lambda a, b: a * b)))
print(list(accumulate([1, 2, 3], initial=10)))
print([(k, list(g)) for k, g in groupby("aaabbbcc")])
print([(k, list(g)) for k, g in groupby([1, 2, 3, 4], key=lambda x: x % 2)])
print([(k, list(g)) for k, g in groupby([])])

a, b = tee([1, 2, 3])
print(list(a), list(b), len(tee("ab", 3)))

print(type(count(1)).__name__, type(chain()).__name__, type(islice("", 0)).__name__)


def gen():
    yield 1
    yield 2


print(list(chain(gen(), [3])), list(takewhile(lambda x: True, gen())))
print(list(islice(gen(), 1)), sorted(set(product(gen(), "a"))))

for call in ("islice('abc', -1)", "islice('abc', 0, 3, 0)", "count(1, 2, 3)",
             "combinations('ab')", "product('ab', repeat=-1)"):
    try:
        eval(call)
        print(call, "no error")
    except Exception as e:
        print(call, type(e).__name__)
