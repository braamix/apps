# The small modules of the first wave: heapq, bisect, keyword, numbers,
# reprlib, __future__, operator and linecache without a file.
import bisect
import heapq
import keyword
import numbers
import operator
import reprlib
import __future__
import linecache

h = [5, 1, 4, 2, 3]
heapq.heapify(h)
print([heapq.heappop(h) for _ in range(3)], heapq.nlargest(2, [3, 9, 1, 7]), heapq.nsmallest(2, "dbca"))
print(list(heapq.merge([1, 4, 7], [2, 5], [3])), heapq.heappushpop([2, 3], 1), heapq.heapreplace([2, 3], 9))
s = [1, 3, 3, 5]
print(bisect.bisect_left(s, 3), bisect.bisect_right(s, 3), bisect.bisect(s, 4, key=None))
bisect.insort(s, 4)
print(s, bisect.bisect_left(["a", "bb", "ccc"], 2, key=len))
print(keyword.iskeyword("for"), keyword.iskeyword("match"), keyword.issoftkeyword("match"),
      len(keyword.kwlist) > 30, "lazy" in keyword.softkwlist)
print(isinstance(1, numbers.Integral), isinstance(1.5, numbers.Rational), isinstance(1j, numbers.Real),
      isinstance(2.5, numbers.Real), issubclass(bool, numbers.Number), isinstance("1", numbers.Number))
print(reprlib.repr(list(range(100))), reprlib.repr("x" * 100), reprlib.repr({i: i for i in range(20)}))


class Loop:
    @reprlib.recursive_repr()
    def __repr__(self):
        return "Loop(" + repr(self.inner) + ")"


lp = Loop()
lp.inner = lp
print(repr(lp), reprlib.Repr(maxlist=2).repr([1, 2, 3]))
print(__future__.annotations, __future__.all_feature_names[:3])
print(operator.add(2, 3), operator.itemgetter(1, 0)("ab"), operator.attrgetter("real")(3),
      operator.methodcaller("upper")("q"), operator.length_hint([1, 2]), operator.is_none(None),
      operator.call(len, "abc"), operator.concat([1], [2]), operator.index(7))
print(linecache.getline("/nowhere.py", 1) == "", linecache.getlines("/nowhere.py"))
