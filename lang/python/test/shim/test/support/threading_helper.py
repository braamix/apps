"""test.support.threading_helper: there is one thread, and a new one cannot be
started, so a test that needs a second skips."""

import contextlib
import unittest


def requires_working_threading(*, module=False):
    msg = "no second thread"
    if module:
        raise unittest.SkipTest(msg)
    return unittest.skip(msg)


def reap_threads(func):
    return func


def threading_setup():
    return ()


def threading_cleanup(*original_values):
    pass


@contextlib.contextmanager
def wait_threads_exit(timeout=None):
    yield


def join_thread(thread, timeout=None):
    thread.join(timeout)


@contextlib.contextmanager
def start_threads(threads, unlock=None):
    raise unittest.SkipTest("no second thread")
    yield


@contextlib.contextmanager
def catch_threading_exception():
    raise unittest.SkipTest("no second thread")
    yield
