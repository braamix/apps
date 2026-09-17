# Traceback objects and the traceback module over them. Only what carries no
# column: the carets under a line are ast's to place, and ast is not here.
import sys
import traceback
import types


def where(e):
    return [(f.name, f.lineno, f.line) for f in traceback.extract_tb(e.__traceback__)]


def outer():
    try:
        inner()
    except ValueError as e:
        raise RuntimeError("wrapped") from e


def inner():
    raise ValueError("bad")


try:
    outer()
except RuntimeError as e:
    print(where(e), where(e.__cause__))
    print(e.__suppress_context__, repr(e.__cause__), e.__context__ is e.__cause__)
    tb = e.__traceback__
    print(tb.tb_lineno, tb.tb_frame.f_code.co_name, tb.tb_next.tb_frame.f_code.co_name,
          tb.tb_next.tb_next, tb.tb_lasti >= 0)
    lines = traceback.format_exception(e)
    print(lines[-1], "direct cause" in "".join(lines), len(lines))

try:
    try:
        1 / 0
    except ZeroDivisionError as e:
        raise e
except ZeroDivisionError as e:
    print([t[1] for t in where(e)])

try:
    try:
        1 / 0
    except ZeroDivisionError:
        raise
except ZeroDivisionError as e:
    print([t[1] for t in where(e)])

try:
    try:
        {}["a"]
    finally:
        pass
except KeyError as e:
    print(where(e))


def gen():
    yield 1
    raise ValueError("in gen")


try:
    for x in gen():
        pass
except ValueError as e:
    print(where(e))

try:
    list(map(lambda v: 1 // v, [1, 0]))
except ZeroDivisionError as e:
    print([t[:2] for t in where(e)])


class Boom:
    def __init__(self):
        raise OSError("boom")


try:
    Boom()
except OSError as e:
    print(where(e))

try:
    raise KeyError("x") from None
except KeyError:
    t, v, tb = sys.exc_info()
    print(t, v.__suppress_context__, v.__cause__, type(tb) is types.TracebackType)
    print(tb is v.__traceback__, traceback.format_exc().splitlines()[-1])

try:
    try:
        raise TypeError("first")
    except TypeError:
        raise ValueError("second")
except ValueError as e:
    print(repr(e.__context__), e.__suppress_context__, where(e.__context__))
    print(sys.exception())
print(sys.exception(), sys.exc_info())

e = ValueError("v")
print(e.__traceback__, e.__suppress_context__)
tb = types.TracebackType(None, sys._getframe(), 4, 99)
print(tb.tb_lineno, tb.tb_lasti, tb.tb_next, e.with_traceback(tb) is e, e.__traceback__ is tb)
tb.tb_next = types.TracebackType(None, sys._getframe(), 0, 7)
print(tb.tb_next.tb_lineno)
for bad in ("x", 1):
    try:
        e.__traceback__ = bad
    except TypeError as err:
        print(err)
try:
    tb.tb_next.tb_next = tb
except ValueError as err:
    print(err)
try:
    tb.tb_lineno = 3
except AttributeError as err:
    print(err)
e.__traceback__ = None
e.__suppress_context__ = True
print(e.__traceback__, e.__suppress_context__)

print(traceback.format_exception_only(ValueError, ValueError("v")))
print(traceback.format_exception_only(ValueError("w")))
print(traceback.format_exception_only(KeyError("k")))
try:
    compile("a = 1 +\n", "x.py", "exec")
except SyntaxError as err:
    print(err.filename, err.lineno, err.text)

s = traceback.StackSummary.extract(traceback.walk_stack(None), limit=1)
print(len(s), s[0].name, s[0].lineno)
print([f.name for f in traceback.extract_stack(limit=1)])


def deep(n):
    if n == 0:
        raise LookupError("deep")
    deep(n - 1)


try:
    deep(3)
except LookupError as e:
    print([t[:2] for t in where(e)])
    print(traceback.format_tb(e.__traceback__, limit=1)[0].splitlines()[1].strip())
    te = traceback.TracebackException.from_exception(e)
    print(te.exc_type_str, te.stack[-1].name, "".join(te.format_exception_only()), end="")
    traceback.clear_frames(e.__traceback__)

co = where.__code__
print(list(co.co_lines())[-1][2], sum(1 for _ in co.co_positions()) > 0)
f = sys._getframe()
print(f.f_lineno, f.f_code.co_name, f.f_lasti >= 0)
try:
    raise SystemExit(3)
except SystemExit as e:
    print(e.code, SystemExit().code, SystemExit(1, 2).code)
