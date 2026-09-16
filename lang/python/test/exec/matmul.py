# @ and @=, and := and * inside a subscript.
import operator
class M:
    def __init__(self, v): self.v = v
    def __matmul__(self, o): return M(self.v * o.v)
    def __rmatmul__(self, o): return M(o * self.v)
    def __imatmul__(self, o): self.v += o.v; return self
    def __repr__(self): return f"M({self.v})"
a = M(2); b = M(3)
print(a @ b, 5 @ b, operator.matmul(a, b))
a @= b
print(a)
try:
    1 @ 2
except TypeError as e:
    print(e)
x = [10, 20, 30]
print(x[i := 1], i, x[*[0]] if False else 0)
