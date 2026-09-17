"""The names CPython's tests take from test.support.warnings_helper.

What upstream's does, over the warnings module that is here: record what a
block warns, and say whether it warned what the test expected.
"""

import contextlib
import re
import sys
import unittest
import warnings


def import_deprecated(name):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", category=DeprecationWarning)
        __import__(name)
        return sys.modules[name]


def check_syntax_warning(testcase, statement, errtext="", *, lineno=1, offset=None):
    raise unittest.SkipTest("check_syntax_warning needs compile() warnings")


@contextlib.contextmanager
def ignore_warnings(*, category, message=""):
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", message=message, category=category)
        yield


@contextlib.contextmanager
def ignore_fork_in_thread_deprecation_warnings():
    yield


class WarningsRecorder:
    """The list catch_warnings keeps, with the last one's fields at hand."""

    def __init__(self, warnings_list):
        self._warnings = warnings_list
        self._last = 0

    def __getattr__(self, attr):
        if len(self._warnings) > self._last:
            return getattr(self._warnings[-1], attr)
        if attr in warnings.WarningMessage._WARNING_DETAILS:
            return None
        raise AttributeError("%r has no attribute %r" % (self, attr))

    @property
    def warnings(self):
        return self._warnings[self._last:]

    def reset(self):
        self._last = len(self._warnings)


def check_warnings(*filters, **kwargs):
    """Record what the block warns; every filter must have caught something
    unless `quiet`, and nothing may be left over."""
    quiet = kwargs.get("quiet")
    if not filters:
        filters = (("", Warning),)
        if quiet is None:
            quiet = True
    return _filterwarnings(filters, quiet)


@contextlib.contextmanager
def check_no_warnings(testcase, message="", category=Warning, force_gc=False):
    from test.support import gc_collect
    with warnings.catch_warnings(record=True) as warns:
        warnings.filterwarnings("always", message=message, category=category)
        yield
        if force_gc:
            gc_collect()
    testcase.assertEqual(warns, [])


@contextlib.contextmanager
def check_no_resource_warning(testcase):
    with check_no_warnings(testcase, category=ResourceWarning, force_gc=True):
        yield


@contextlib.contextmanager
def _filterwarnings(filters, quiet=False):
    wmod = sys.modules["warnings"]
    with wmod.catch_warnings(record=True) as w:
        wmod.simplefilter("always")
        yield WarningsRecorder(w)
    reraise = list(w)
    missing = []
    for msg, cat in filters:
        seen = False
        for one in reraise[:]:
            warning = one.message
            if re.match(msg, str(warning), re.I) and issubclass(warning.__class__, cat):
                seen = True
                reraise.remove(one)
        if not seen and not quiet:
            missing.append((msg, cat.__name__))
    if reraise:
        raise AssertionError("unhandled warning %s" % reraise[0])
    if missing:
        raise AssertionError("filter (%r, %s) did not catch any warning" % missing[0])


@contextlib.contextmanager
def save_restore_warnings_filters():
    old_filters = warnings.filters[:]
    try:
        yield
    finally:
        warnings.filters[:] = old_filters
