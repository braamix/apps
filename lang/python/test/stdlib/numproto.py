# The number protocol reaching Python: __ne__ through __eq__, __index__,
# __float__, __complex__ and __divmod__.
import operator
class A:
    def __eq__(s, o): return o == 5
print(A() != 5, A() != 4, A() == 5)
class B:
    def __index__(s): return 7
    def __float__(s): return 2.5
    def __divmod__(s, o): return ("dm", o)
    def __rdivmod__(s, o): return ("rdm", o)
    def __complex__(s): return 1j
print(operator.index(B()), float(B()), divmod(B(), 2), divmod(3, B()), complex(B()), [0, 1, 2, 3, 4, 5, 6, 7][B()])
class C:
    def __float__(s): return 1.5
print(complex(C()), int.__index__(3), round(2.5), abs(-2))
