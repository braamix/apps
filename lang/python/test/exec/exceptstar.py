# except*: matching, re-raising, what the clauses raise, and what is refused.
def run(f):
    try:
        f()
    except BaseException as e:
        print("escaped:", type(e).__name__, repr(e))


def t1():
    try:
        raise ExceptionGroup("eg", [ValueError(1), TypeError(2), KeyError(3)])
    except* ValueError as e:
        print("V", repr(e))
    except* (TypeError, KeyError) as e:
        print("TK", repr(e))


def t2():
    try:
        raise ExceptionGroup("eg", [ValueError(1), OSError(2)])
    except* ValueError:
        print("only V")


def t3():
    try:
        raise ValueError("naked")
    except* ValueError as e:
        print("naked caught", repr(e), repr(e.exceptions[0]))


def t4():
    try:
        raise TypeError("naked, unmatched")
    except* ValueError:
        print("no")


def t5():
    try:
        raise ExceptionGroup("eg", [ValueError(1), ExceptionGroup("in", [TypeError(2)])])
    except* ValueError as e:
        print("V", repr(e))
        raise KeyError(5)
    except* TypeError:
        print("T handled")


def t6():
    try:
        raise ExceptionGroup("eg", [ValueError(1), TypeError(2)])
    except* ValueError:
        raise
    except* TypeError:
        raise


def t7():
    try:
        raise ExceptionGroup("eg", [ValueError(1), TypeError(2)])
    except* ValueError:
        raise KeyError("a")
    except* TypeError:
        raise OSError("b")


def t8():
    try:
        try:
            raise ExceptionGroup("eg", [ValueError(1)])
        except* ExceptionGroup:
            pass
    except TypeError as e:
        print("TypeError:", e)


def t9():
    try:
        pass
    except* ValueError:
        print("no")
    else:
        print("else ran")
    finally:
        print("finally ran")


def t10():
    try:
        raise ExceptionGroup("eg", [ValueError(1)])
    except* ValueError:
        print("in handler")
    finally:
        print("finally after")


def t11():
    for i in range(2):
        try:
            raise ExceptionGroup("eg", [ValueError(i)])
        except* ValueError as e:
            print("loop", repr(e))
    try:
        e
    except NameError:
        print("e unbound")


def t12():
    import sys
    try:
        raise ExceptionGroup("eg", [ValueError(1), TypeError(2)])
    except* ValueError:
        print("handling", repr(sys.exception()))


def t13():
    try:
        raise ExceptionGroup("eg", [KeyboardInterrupt(), ValueError(1)])
    except* KeyboardInterrupt:
        print("KI")


def t14():
    try:
        raise ExceptionGroup("eg", [ValueError(1)])
    except* 42:
        pass


for t in [t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14]:
    print("--", t.__name__)
    run(t)

for code in ["try: pass\nexcept* ValueError: pass\nexcept TypeError: pass",
             "try: pass\nexcept*: pass",
             "try: pass\nexcept ValueError, TypeError as e: pass",
             "for x in y:\n    try: pass\n    except* ValueError: break",
             "def f():\n    try: pass\n    except* ValueError: return"]:
    try:
        compile(code, "s", "exec")
    except SyntaxError as e:
        print(e.msg)
try:
    raise TypeError("pep 758")
except ValueError, TypeError:
    print("unparenthesized")
