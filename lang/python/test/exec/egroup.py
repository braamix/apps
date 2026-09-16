# Exception groups: construction, str and repr, split, subgroup, derive.
eg = ExceptionGroup("msg", [ValueError(1), TypeError("x")])
print(str(eg), repr(eg), eg.args, eg.message, eg.exceptions)
print(repr(ExceptionGroup("m", (ValueError(1),))), BaseExceptionGroup("m", [ValueError(1)]).__class__, BaseExceptionGroup("m", [KeyboardInterrupt()]).__class__)
print(ExceptionGroup.__mro__, BaseExceptionGroup.__mro__)
print(isinstance(eg, Exception), isinstance(eg, BaseExceptionGroup), issubclass(ExceptionGroup, Exception))
e = ValueError(1); e.add_note("a note"); print(e.__notes__)
m, r = eg.split(ValueError); print(repr(m), repr(r))
print(repr(eg.subgroup(lambda e: isinstance(e, TypeError))))
print(eg.split(Exception)[0] is eg, eg.subgroup(OSError))
nested = ExceptionGroup("outer", [ValueError(1), ExceptionGroup("inner", [TypeError(2), ValueError(3)])])
print(repr(nested.split(ValueError)))
print(repr(nested.split(lambda e: isinstance(e, ValueError))))
print(repr(nested.subgroup((TypeError, KeyError))))
for bad in [(1,), ("m", 1), ("m", []), ("m", [1]), (1, [ValueError()])]:
    try: ExceptionGroup(*bad)
    except Exception as x: print(type(x).__name__, x)
try: ExceptionGroup("m", [KeyboardInterrupt()])
except Exception as x: print(type(x).__name__, x)
try: eg.split(1)
except Exception as x: print(type(x).__name__, x)
print(ExceptionGroup[int])
nested.__cause__ = KeyError("c")
nested.add_note("top note")
m, r = nested.split(TypeError)
print(repr(m.__cause__), m.__notes__, m.__notes__ is nested.__notes__, repr(r.exceptions))
class MyEG(ExceptionGroup):
    def __new__(cls, message, excs, code):
        obj = super().__new__(cls, message, excs)
        obj.code = code
        return obj
    def derive(self, excs):
        return MyEG(self.message, excs, self.code)
g = MyEG("mine", [ValueError(1), TypeError(2)], 42)
print(type(g).__name__, g.args, g.code, str(g))
m, r = g.split(ValueError)
print(type(m).__name__, m.code, repr(m.exceptions), type(r).__name__, r.code)
class Plain(ExceptionGroup):
    pass
p = Plain("p", [ValueError(1)])
print(repr(p), type(p.split(ValueError)[0]).__name__)
try:
    raise eg
except ExceptionGroup as caught:
    print("caught", caught is eg)
try:
    raise eg
except Exception as caught:
    print("caught as Exception")
