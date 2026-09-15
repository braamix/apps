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


def captured_stdout():
    raise unittest.SkipTest("captured_stdout needs io")


def captured_stderr():
    raise unittest.SkipTest("captured_stderr needs io")


def swap_attr(obj, name, new_val):
    raise unittest.SkipTest("swap_attr needs a context manager over contextlib")


def findfile(filename, subdir=None):
    raise unittest.SkipTest("findfile needs os.path")


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
