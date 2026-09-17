# dict.fromkeys makes the class it is called on.
from collections import OrderedDict
class D(dict):
    def __setitem__(s, k, v):
        print("set", k)
        super().__setitem__(k, v * 2)
print(type(OrderedDict.fromkeys("ab")).__name__, D.fromkeys([1, 2], 5), dict.fromkeys(x for x in "xy"), frozendict.fromkeys("q", 1), type({}.fromkeys([])).__name__)
