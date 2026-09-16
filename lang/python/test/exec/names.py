# Private names, a class's own namespace, and where := binds.
class A:
    __x = 1
    __y__ = 2
    def f(self, __p=5):
        self.__z = __p
        return self.__x, self.__z
    def __g(self):
        return "g"
    def h(self):
        return self.__g()
    class __Inner:
        pass
print(A._A__x, A().f(), A().h(), A._A__Inner.__name__, A.__y__)
print(sorted(k for k in vars(A) if "__" in k and not k.startswith("__")))
class _B:
    def m(self):
        __q = 3
        return [__q for _ in range(1)], (lambda: __q)()
print(_B().m())
class __C:
    __v = 1
print(sorted(k for k in vars(__C) if "v" in k))
class D:
    __slots__ = ("__s",)
    def __init__(self):
        self.__s = 9
print(D().__dict__ if hasattr(D(), "__dict__") else D()._D__s)
def outer():
    class E:
        def get(self):
            return __name__
    return E().get()
print(outer())

class C:
    print(sorted(k for k in locals()))
print(C.__module__, C.__qualname__, '__module__' in C.__dict__)
def f():
    class D: pass
    return D
print(f().__qualname__, int.__module__, C.__doc__, type(None).__module__)
class E:
    "doc"
print(E.__doc__, sorted(k for k in E.__dict__ if k.startswith('__') and k not in ('__dict__', '__weakref__')))
class G:
    def __init__(self): self.x = 1; self.y = 2
    def f(s): s.z = 3
    @staticmethod
    def g(a): a.w = 1
print(G.__static_attributes__, C.__static_attributes__)

# := inside comprehensions binds in the enclosing scope (PEP 572)
def scope():
    [total := 0 for _ in range(1)]
    [(total := total + v) for v in range(5)]
    return total
print(scope())
[mod := i for i in range(3)]
print(mod)
def nested():
    return [[(n := n0 + n1) for n1 in range(2)] for n0 in range(2)], n
print(nested())
for code in ["[i := 0 for i in range(3)]", "[x for x in (y := [1])]",
             "class A: [z := 1 for _ in range(1)]",
             "[j for i in range(3) if (j := i) for j in range(2)]", "x := 1",
             "f(a=1, b)", "f(**k, *a)", "((a, b) := (1, 2))", "(lambda: x := 1)"]:
    try:
        exec(code)
    except SyntaxError as e:
        print(e.msg)
