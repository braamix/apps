# A class with only __getitem__ is iterable: walked from 0 until IndexError.
class S:
    def __getitem__(self, i):
        if i > 3: raise IndexError
        return i * i
print(list(S()), [x for x in S()], sum(S()), 9 in S(), tuple(S()))
a, b, *c = S()
print(a, b, c)
it = iter(S())
print(next(it), next(it), list(it), next(it, "done"))
for x in S():
    print(x, end=" ")
print()
class T:
    def __getitem__(self, i):
        if i == 2: raise StopIteration
        return i
print(list(T()))
