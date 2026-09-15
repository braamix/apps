"""Enough of unittest to run CPython's own tests.

Not a copy of upstream's: the real one imports asyncio, logging, argparse and
inspect, none of which exist here yet. This is the surface the tests in
test/cpython/ actually touch, written in the Python this interpreter has --
no f-strings, no %, no str.format, no generators, no dir(), no globals(),
and no attributes on a function object.

main() prints a fenced listing the harness reads:

    --- unittest ---
    ok ClassName.method
    fail ClassName.method: AssertionError: ...
    --- ran 3 ok 2 fail 1 error 0 skip 0 ---
"""

import sys


class SkipTest(Exception):
    pass


class _ShimLimit(Exception):
    """What the shim itself cannot honour. Never a test's own failure."""


# A decorator cannot mark a function here -- a function object takes no
# attributes -- so a skipped test is replaced in the class body by one of
# these, and the loader recognises it by type.
class _Marker:
    def __init__(self, kind, reason, wrapped):
        self.kind = kind
        self.reason = reason
        self.wrapped = wrapped


def _mark(kind, reason, obj):
    # A class keeps its identity, or the loader would not find it at all and
    # its tests would vanish rather than show as skipped.
    if isinstance(obj, type):
        obj._shim_skip = reason
        return obj
    return _Marker(kind, reason, obj)


def skip(reason):
    def deco(obj):
        return _mark("skip", reason, obj)
    return deco


def skipIf(condition, reason):
    def deco(obj):
        if condition:
            return _mark("skip", reason, obj)
        return obj
    return deco


def skipUnless(condition, reason):
    def deco(obj):
        if not condition:
            return _mark("skip", reason, obj)
        return obj
    return deco


def expectedFailure(obj):
    return _Marker("expected", "expected failure", obj)


# A class decorated whole. The loader looks for this on the class itself.
def _class_reason(cls):
    return getattr(cls, "_shim_skip", None)


class _Raises:
    """assertRaises and assertRaisesRegex, as a context manager or a call."""

    def __init__(self, case, expected, pattern):
        self.case = case
        self.expected = expected
        self.pattern = pattern
        self.exception = None

    def __enter__(self):
        return self

    def __exit__(self, ty, val, tb):
        if ty is None:
            self.case.fail(_name_of(self.expected) + " not raised")
        if not _is_expected(ty, self.expected):
            return False            # let the unexpected one through
        self.exception = val
        if self.pattern is not None:
            text = str(val)
            if not _search(self.pattern, text):
                self.case.fail(repr(self.pattern) + " does not match " + repr(text))
        return True


def _is_expected(ty, expected):
    if isinstance(expected, tuple):
        for e in expected:
            if issubclass(ty, e):
                return True
        return False
    return issubclass(ty, expected)


def _name_of(expected):
    if isinstance(expected, tuple):
        parts = []
        for e in expected:
            parts.append(e.__name__)
        return "(" + ", ".join(parts) + ")"
    return expected.__name__


_META = "\\.*+?[]{}()|"


def _search(pattern, text):
    """re.search over the subset CPython's tests here actually use: literal
    text, ^, $, and a backslash escaping a punctuation character. A pattern
    wanting more raises rather than guessing."""
    start = False
    end = False
    body = pattern
    if body.startswith("^"):
        start = True
        body = body[1:]
    if body.endswith("$") and not body.endswith("\\$"):
        end = True
        body = body[:-1]

    lit = ""
    i = 0
    n = len(body)
    while i < n:
        c = body[i]
        if c == "\\":
            if i + 1 >= n:
                raise _ShimLimit("trailing backslash in " + repr(pattern))
            nxt = body[i + 1]
            if nxt.isalnum():
                raise _ShimLimit("escape \\" + nxt + " in " + repr(pattern))
            lit = lit + nxt
            i = i + 2
            continue
        if c in _META:
            raise _ShimLimit("metacharacter " + repr(c) + " in " + repr(pattern))
        lit = lit + c
        i = i + 1

    if start and end:
        return text == lit
    if start:
        return text.startswith(lit)
    if end:
        return text.endswith(lit)
    return lit in text


class _SubTest:
    def __init__(self, case, label):
        self.case = case
        self.label = label

    def __enter__(self):
        return self

    def __exit__(self, ty, val, tb):
        if ty is None:
            return False
        if issubclass(ty, SkipTest):
            return True
        self.case._subfail.append(self.label + ": " + _describe(val))
        return True             # CPython reports it and carries on


def _describe(exc):
    text = str(exc)
    if text:
        return type(exc).__name__ + ": " + text
    return type(exc).__name__


class TestCase:
    failureException = AssertionError
    longMessage = True
    maxDiff = 640

    def __init__(self, methodName="runTest"):
        self._testMethodName = methodName
        self._subfail = []

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def setUpClass():
        pass
    setUpClass = staticmethod(setUpClass)

    def tearDownClass():
        pass
    tearDownClass = staticmethod(tearDownClass)

    def id(self):
        return type(self).__name__ + "." + self._testMethodName

    def shortDescription(self):
        return None

    # -- the failures ----------------------------------------------------

    def fail(self, msg=None):
        raise self.failureException(msg if msg is not None else "fail")

    def skipTest(self, reason):
        raise SkipTest(reason)

    def _fail(self, standard, msg):
        if msg is None:
            self.fail(standard)
        if self.longMessage:
            self.fail(standard + " : " + str(msg))
        self.fail(str(msg))

    # -- the assertions --------------------------------------------------

    def assertTrue(self, expr, msg=None):
        if not expr:
            self._fail(repr(expr) + " is not true", msg)

    def assertFalse(self, expr, msg=None):
        if expr:
            self._fail(repr(expr) + " is not false", msg)

    def assertEqual(self, a, b, msg=None):
        if not a == b:
            self._fail(_diff(a, b, "!="), msg)

    def assertNotEqual(self, a, b, msg=None):
        if a == b:
            self._fail(_diff(a, b, "=="), msg)

    def assertIs(self, a, b, msg=None):
        if a is not b:
            self._fail(_diff(a, b, "is not"), msg)

    def assertIsNot(self, a, b, msg=None):
        if a is b:
            self._fail("unexpectedly identical: " + repr(a), msg)

    def assertIsNone(self, a, msg=None):
        if a is not None:
            self._fail(repr(a) + " is not None", msg)

    def assertIsNotNone(self, a, msg=None):
        if a is None:
            self._fail("unexpectedly None", msg)

    def assertIn(self, a, b, msg=None):
        if a not in b:
            self._fail(_diff(a, b, "not found in"), msg)

    def assertNotIn(self, a, b, msg=None):
        if a in b:
            self._fail(_diff(a, b, "unexpectedly found in"), msg)

    def assertIsInstance(self, obj, cls, msg=None):
        if not isinstance(obj, cls):
            self._fail(repr(obj) + " is not an instance of " + repr(cls), msg)

    def assertNotIsInstance(self, obj, cls, msg=None):
        if isinstance(obj, cls):
            self._fail(repr(obj) + " is an instance of " + repr(cls), msg)

    def assertHasAttr(self, obj, name, msg=None):
        if not hasattr(obj, name):
            self._fail(repr(obj) + " has no attribute " + repr(name), msg)

    def assertNotHasAttr(self, obj, name, msg=None):
        if hasattr(obj, name):
            self._fail(repr(obj) + " has unexpected attribute " + repr(name), msg)

    def assertGreater(self, a, b, msg=None):
        if not a > b:
            self._fail(_diff(a, b, "not greater than"), msg)

    def assertGreaterEqual(self, a, b, msg=None):
        if not a >= b:
            self._fail(_diff(a, b, "not greater than or equal to"), msg)

    def assertLess(self, a, b, msg=None):
        if not a < b:
            self._fail(_diff(a, b, "not less than"), msg)

    def assertLessEqual(self, a, b, msg=None):
        if not a <= b:
            self._fail(_diff(a, b, "not less than or equal to"), msg)

    def assertAlmostEqual(self, a, b, places=None, msg=None, delta=None):
        if a == b:
            return
        if delta is not None:
            if abs(a - b) <= delta:
                return
            self._fail(_diff(a, b, "!=") + " within " + repr(delta) + " delta", msg)
        if places is None:
            places = 7
        if round(abs(b - a), places) == 0:
            return
        self._fail(_diff(a, b, "!=") + " within " + str(places) + " places", msg)

    def assertNotAlmostEqual(self, a, b, places=None, msg=None, delta=None):
        if delta is not None:
            if not a == b and abs(a - b) > delta:
                return
            self._fail(_diff(a, b, "==") + " within " + repr(delta) + " delta", msg)
        if places is None:
            places = 7
        if not a == b and round(abs(b - a), places) != 0:
            return
        self._fail(_diff(a, b, "==") + " within " + str(places) + " places", msg)

    def assertSequenceEqual(self, a, b, msg=None, seq_type=None):
        self.assertEqual(list(a), list(b), msg)

    def assertListEqual(self, a, b, msg=None):
        self.assertEqual(a, b, msg)

    def assertTupleEqual(self, a, b, msg=None):
        self.assertEqual(a, b, msg)

    def assertDictEqual(self, a, b, msg=None):
        self.assertEqual(a, b, msg)

    def assertSetEqual(self, a, b, msg=None):
        self.assertEqual(set(a), set(b), msg)

    def assertMultiLineEqual(self, a, b, msg=None):
        self.assertEqual(a, b, msg)

    def assertCountEqual(self, a, b, msg=None):
        self.assertEqual(sorted(a), sorted(b), msg)

    def assertRaises(self, expected, *args, **kwargs):
        ctx = _Raises(self, expected, None)
        if not args:
            return ctx
        return _call_in(ctx, args, kwargs)

    def assertRaisesRegex(self, expected, pattern, *args, **kwargs):
        ctx = _Raises(self, expected, pattern)
        if not args:
            return ctx
        return _call_in(ctx, args, kwargs)

    def subTest(self, msg=None, **params):
        parts = []
        if msg is not None:
            parts.append("[" + str(msg) + "]")
        keys = sorted([k for k in params])
        for k in keys:
            parts.append(k + "=" + repr(params[k]))
        return _SubTest(self, " ".join(parts) if parts else "[subtest]")

    # -- the runner's entry ----------------------------------------------

    def run(self, func=None):
        """One test: ('ok'|'fail'|'error'|'skip', detail). `func` is the
        unbound function for a test the class dict holds a marker for, which
        getattr would hand back instead of the method."""
        try:
            self.setUp()
        except SkipTest as e:
            return ("skip", str(e))
        except BaseException as e:
            return ("error", "setUp: " + _describe(e))

        state = ("ok", "")
        try:
            if func is None:
                getattr(self, self._testMethodName)()
            else:
                func(self)
        except SkipTest as e:
            state = ("skip", str(e))
        except self.failureException as e:
            state = ("fail", _describe(e))
        except BaseException as e:
            state = ("error", _describe(e))

        try:
            self.tearDown()
        except BaseException as e:
            if state[0] == "ok":
                state = ("error", "tearDown: " + _describe(e))

        if state[0] == "ok" and self._subfail:
            state = ("fail", str(len(self._subfail)) + " subtests: " +
                     "; ".join(self._subfail))
        return state


def _call_in(ctx, args, kwargs):
    """assertRaises(Exc, fn, *rest) -- the call form, over the same object."""
    fn = args[0]
    rest = args[1:]
    ctx.__enter__()
    try:
        fn(*rest, **kwargs)
    except BaseException as e:
        if ctx.__exit__(type(e), e, None):
            return None
        raise
    ctx.__exit__(None, None, None)
    return None


def _diff(a, b, verb):
    return _short(repr(a)) + " " + verb + " " + _short(repr(b))


def _short(text):
    if len(text) <= 200:
        return text
    return text[:200] + "..."


# -- discovery and the run ----------------------------------------------

def _methods(cls):
    """Every test method name on a class and its bases, deduplicated."""
    seen = {}
    order = []
    stack = [cls]
    while stack:
        c = stack.pop(0)
        for name in c.__dict__:
            if name.startswith("test") and name not in seen:
                seen[name] = True
                order.append(name)
        for b in c.__bases__:
            stack.append(b)
    return sorted(order)


def _cases(namespace):
    """The TestCase subclasses in a module namespace, by name."""
    found = []
    for name in namespace:
        v = namespace[name]
        if isinstance(v, type) and v is not TestCase and issubclass(v, TestCase):
            found.append(name)
    out = []
    for name in sorted(found):
        out.append((name, namespace[name]))
    return out


class TestResult:
    def __init__(self):
        self.ran = 0
        self.ok = 0
        self.failures = []
        self.errors = []
        self.skipped = []

    def wasSuccessful(self):
        return not self.failures and not self.errors


def _run_class(name, cls, result, lines):
    reason = _class_reason(cls)
    for method in _methods(cls):
        label = name + "." + method
        result.ran = result.ran + 1
        if reason is not None:
            result.skipped.append(label)
            lines.append("skip " + label + ": " + reason)
            continue
        entry = _find(cls, method)
        if isinstance(entry, _Marker) and entry.kind == "skip":
            result.skipped.append(label)
            lines.append("skip " + label + ": " + entry.reason)
            continue

        expected = isinstance(entry, _Marker) and entry.kind == "expected"
        try:
            case = cls(method)
        except BaseException as e:
            result.errors.append(label)
            lines.append("error " + label + ": __init__: " + _describe(e))
            continue
        state, detail = case.run(entry.wrapped if expected else None)

        if expected:
            if state == "ok":
                result.failures.append(label)
                lines.append("fail " + label + ": unexpectedly passed")
            else:
                result.ok = result.ok + 1
                lines.append("ok " + label + " (expected failure)")
            continue

        if state == "ok":
            result.ok = result.ok + 1
            lines.append("ok " + label)
        elif state == "skip":
            result.skipped.append(label)
            lines.append("skip " + label + ": " + detail)
        elif state == "fail":
            result.failures.append(label)
            lines.append("fail " + label + ": " + detail)
        else:
            result.errors.append(label)
            lines.append("error " + label + ": " + detail)


def _find(cls, name):
    """The raw class-dict entry, which getattr would turn into a method."""
    stack = [cls]
    while stack:
        c = stack.pop(0)
        if name in c.__dict__:
            return c.__dict__[name]
        for b in c.__bases__:
            stack.append(b)
    return None


def main(*args, **kwargs):
    namespace = sys.modules["__main__"].__dict__
    result = TestResult()
    lines = []
    for name, cls in _cases(namespace):
        try:
            cls.setUpClass()
        except SkipTest as e:
            for method in _methods(cls):
                result.ran = result.ran + 1
                result.skipped.append(name + "." + method)
                lines.append("skip " + name + "." + method + ": " + str(e))
            continue
        except BaseException as e:
            for method in _methods(cls):
                result.ran = result.ran + 1
                result.errors.append(name + "." + method)
                lines.append("error " + name + "." + method +
                             ": setUpClass: " + _describe(e))
            continue
        _run_class(name, cls, result, lines)
        try:
            cls.tearDownClass()
        except BaseException:
            pass

    print("--- unittest ---")
    for line in lines:
        print(line)
    print("--- ran " + str(result.ran) +
          " ok " + str(result.ok) +
          " fail " + str(len(result.failures)) +
          " error " + str(len(result.errors)) +
          " skip " + str(len(result.skipped)) + " ---")
    return result
