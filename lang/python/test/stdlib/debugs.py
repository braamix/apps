# __debug__, compile()'s optimize, the integer string limit, breakpoint() and
# the builtins site adds.
import os
import sys
import warnings

for src in ["__debug__ = 1", "del __debug__", "x.__debug__ = 1", "f(__debug__=1)",
            "def f(__debug__): pass", "import os as __debug__", "global __debug__",
            "def f(*, __debug__): pass", "class __debug__: pass", "del x.__debug__",
            "__debug__: int", "for __debug__ in []: pass", "lambda __debug__: 0",
            "x = __debug__", "(__debug__ := 1)", "with a as __debug__: pass"]:
    try:
        compile(src, "f", "exec")
        print(repr(src), "ok")
    except SyntaxError as e:
        print(repr(src), e.msg)

print(__debug__, eval(compile("__debug__", "f", "eval", optimize=1)),
      eval(compile("__debug__", "f", "eval", optimize=0)))
exec(compile("assert 0", "f", "exec", optimize=1))
ns = {}
exec(compile("def g():\n    'doc'\n", "f", "exec", optimize=2), ns)
print(ns["g"].__doc__)
for bad in [3, -2]:
    try:
        compile("1", "f", "exec", optimize=bad)
    except ValueError as e:
        print(e)

print(sys.get_int_max_str_digits())
for f in (lambda: int("1" * 5000), lambda: str(10 ** 5000), lambda: f"{10 ** 5000:d}",
          lambda: len(hex(10 ** 5000)), lambda: len(str(10 ** 4000))):
    try:
        print(f())
    except ValueError as e:
        print(e)
sys.set_int_max_str_digits(640)
try:
    int("9" * 641)
except ValueError as e:
    print(e)
sys.set_int_max_str_digits(maxdigits=0)
print(len(str(10 ** 5000)), sys.get_int_max_str_digits())
for bad in [5, -1]:
    try:
        sys.set_int_max_str_digits(bad)
    except ValueError as e:
        print(e)
sys.set_int_max_str_digits(4300)

print(sys.breakpointhook is sys.__breakpointhook__, breakpoint.__name__)


def mine(*a, **k):
    print("mine", a, k)
    return 7


sys.breakpointhook = mine
print(breakpoint(1, x=2))
sys.breakpointhook = sys.__breakpointhook__
os.environ["PYTHONBREAKPOINT"] = "0"
print(breakpoint())
os.environ["PYTHONBREAKPOINT"] = "builtins.print"
print(breakpoint("via", "env", sep="-"))
os.environ["PYTHONBREAKPOINT"] = "len"
print(breakpoint("abc"))
for name in ["nosuch.mod", "os.nosuch"]:
    os.environ["PYTHONBREAKPOINT"] = name
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")
        print(breakpoint())
    print([str(x.message) for x in w], [x.category.__name__ for x in w])
del sys.breakpointhook
try:
    breakpoint()
except RuntimeError as e:
    print(e)

print(repr(exit), repr(quit), repr(copyright)[:60], type(credits).__name__)
try:
    exit(5)
except SystemExit as e:
    print("SystemExit", e.code)
