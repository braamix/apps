"""The names test/cpython/'s tests take from test.support.

Upstream's is 3,000 lines over os, io, socket, subprocess and threading. This
is the part those tests reach, and each name answers for this platform rather
than pretending to be CPython's: there is no refcount, no C docstrings and no
subinterpreter here, so the decorators that guard on them skip.
"""

import sys
import unittest

verbose = 1
is_wasi = False
is_emscripten = False
is_android = False
is_apple = False
is_apple_mobile = False

# Braam is one Web Worker; a process cannot fork and there is no second thread.
has_fork_support = False
has_subprocess_support = False
has_socket_support = False
has_strftime_extensions = False

# A docstring is dropped by the compiler here, so anything testing one skips.
HAVE_PY_DOCSTRINGS = False
HAVE_DOCSTRINGS = False
MISSING_C_DOCSTRINGS = True

# Mark-and-sweep, so sys.gettotalrefcount does not exist and cannot.
Py_DEBUG = False

TESTFN = "@test"
TESTFN_ASCII = "@test"

_1M = 1024 * 1024
_1G = 1024 * _1M
_2G = 2 * _1G
_4G = 4 * _1G

SHORT_TIMEOUT = 30.0
LONG_TIMEOUT = 5 * 60.0


class Error(Exception):
    pass


class TestFailed(Error):
    pass


class ResourceDenied(unittest.SkipTest):
    pass


def get_attribute(obj, name):
    """getattr, but a missing name skips the test rather than failing it."""
    try:
        return getattr(obj, name)
    except AttributeError:
        raise unittest.SkipTest("object " + repr(obj) + " has no attribute " + repr(name))


def disable_gc():
    """The collector off for the block, and on again after if it was."""
    import contextlib
    import gc

    @contextlib.contextmanager
    def manager():
        have = gc.isenabled()
        gc.disable()
        try:
            yield
        finally:
            if have:
                gc.enable()

    return manager()


def gc_collect():
    """A collection, and everything unreachable gone after it."""
    try:
        import gc
    except ImportError:
        return
    gc.collect()


def requires(resource, msg=None):
    raise ResourceDenied(msg if msg else "resource " + repr(resource) + " is not enabled")


def requires_resource(resource):
    return unittest.skip("resource " + repr(resource) + " is not enabled")


def check_disallow_instantiation(testcase, tp, *args, **kwds):
    """Calling the type, or its __new__, is refused."""
    import re
    mod = tp.__module__
    name = tp.__name__
    if mod != 'builtins':
        qualname = f"{mod}.{name}"
    else:
        qualname = f"{name}"
    msg = f"cannot create '{re.escape(qualname)}' instances"
    testcase.assertRaisesRegex(TypeError, msg, tp, *args, **kwds)
    testcase.assertRaisesRegex(TypeError, msg, tp.__new__, tp, *args, **kwds)


# The C library is the port's own, not musl's.
def linked_to_musl():
    return False


class Stopwatch:
    """Time a CPU-bound block. The clock may stand still under the harness."""

    def __enter__(self):
        import time
        self.get_time = time.perf_counter
        self.start_time = self.get_time()
        return self

    def __exit__(self, *exc):
        self.stop_time = self.get_time()
        self.seconds = self.stop_time - self.start_time


def run_unittest(*classes):
    return unittest.main()


def check_syntax_error(testcase, statement, errtext="", lineno=None, offset=None):
    raise unittest.SkipTest("check_syntax_error needs compile()")


def check_impl_detail(**guards):
    return guards.get("cpython", True)


def cpython_only(test):
    return unittest.skip("implementation detail of CPython")(test)


def impl_detail(msg=None, **guards):
    def deco(test):
        return test
    return deco


def no_tracing(test):
    return test


def bigmemtest(size, memuse, dry_run=True):
    def deco(test):
        return unittest.skip("not enough memory for a bigmem test")(test)
    return deco


def skip_if_broken_multiprocessing_synchronize():
    raise unittest.SkipTest("no multiprocessing here")


def skip_if_sanitizer(reason=None, **kinds):
    def deco(test):
        return test
    return deco


def subTests(arg_names, arg_values, /, *, _do_cleanups=False):
    raise unittest.SkipTest("subTests needs generators")


def _contextmanager(fn):
    import contextlib
    return contextlib.contextmanager(fn)


@_contextmanager
def captured_output(stream_name):
    """sys.<stream_name> is a StringIO for the block, which it hands out."""
    import io
    orig = getattr(sys, stream_name)
    setattr(sys, stream_name, io.StringIO())
    try:
        yield getattr(sys, stream_name)
    finally:
        setattr(sys, stream_name, orig)


def captured_stdout():
    return captured_output("stdout")


def captured_stderr():
    return captured_output("stderr")


def captured_stdin():
    return captured_output("stdin")


@_contextmanager
def swap_attr(obj, attr, new_val):
    """obj.attr is new_val for the block, and what it was after."""
    if hasattr(obj, attr):
        real_val = getattr(obj, attr)
        setattr(obj, attr, new_val)
        try:
            yield real_val
        finally:
            setattr(obj, attr, real_val)
    else:
        setattr(obj, attr, new_val)
        try:
            yield
        finally:
            if hasattr(obj, attr):
                delattr(obj, attr)


@_contextmanager
def swap_item(obj, item, new_val):
    """obj[item] is new_val for the block, and what it was after."""
    if item in obj:
        real_val = obj[item]
        obj[item] = new_val
        try:
            yield real_val
        finally:
            obj[item] = real_val
    else:
        obj[item] = new_val
        try:
            yield
        finally:
            if item in obj:
                del obj[item]


def _test_home():
    import os
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def findfile(filename, subdir=None):
    """The file under the test package, or on sys.path, or the name itself."""
    import os
    if os.path.isabs(filename):
        return filename
    if subdir is not None:
        filename = os.path.join(subdir, filename)
    for dn in [_test_home()] + sys.path:
        fn = os.path.join(dn, filename)
        if os.path.exists(fn):
            return fn
    return filename


class infinite_recursion:
    """A recursion limit a runaway recursion reaches soon, for a block or a
    decorated test. A class rather than contextlib's decorator, which needs
    inspect."""

    def __init__(self, max_depth=None):
        self.max_depth = 20_000 if max_depth is None else max_depth

    def __enter__(self):
        self.original = sys.getrecursionlimit()
        sys.setrecursionlimit(self.max_depth)
        return self

    def __exit__(self, *exc):
        sys.setrecursionlimit(self.original)
        return False

    def __call__(self, fn):
        def wrapper(*args, **kwds):
            with infinite_recursion(self.max_depth):
                return fn(*args, **kwds)
        wrapper.__name__ = fn.__name__
        return wrapper


# The frames are heap here, so no test can exhaust a C stack: a deep one
# stops at the recursion limit, and these run it as it is.
def skip_emscripten_stack_overflow():
    return unittest.skipIf(is_emscripten, "Exhausts stack on Emscripten")


def skip_wasi_stack_overflow():
    return unittest.skipIf(is_wasi, "Exhausts stack on WASI")


def exceeds_recursion_limit():
    """For recursion tests, easily exceeds default recursion limit."""
    return 150_000


def run_with_limited_c_stack(depth=150_000, size=None):
    def decorator(test):
        return test
    return decorator


# Reference counting is what these guard on, and there is none: the collector
# is mark-and-sweep, so a count is not merely unavailable but meaningless.
refcount_test = unittest.skip("the collector does not count references")

requires_docstrings = unittest.skipUnless(HAVE_DOCSTRINGS, "test requires docstrings")

requires_working_socket = lambda *a, **k: unittest.skip("no sockets")
requires_fork = lambda: unittest.skip("no fork")
requires_subprocess = lambda: unittest.skip("no subprocess")


class _NeverEqual:
    def __eq__(self, other):
        return False

    def __ne__(self, other):
        return True

    def __hash__(self):
        return 1


class _AlwaysEqual:
    def __eq__(self, other):
        return True

    def __ne__(self, other):
        return False

    def __hash__(self):
        return 1


NEVER_EQ = _NeverEqual()
ALWAYS_EQ = _AlwaysEqual()
