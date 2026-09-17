# A metaclass's __repr__ shows its classes.
class M(type):
    def __repr__(c): return "M!"
class A(metaclass=M): pass
print(A, [A], "%r" % A, f"{A!r}", str(A), frozendict(a=A))
