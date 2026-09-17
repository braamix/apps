"""test.support.strace_helper: there is no strace here, so what needs one skips."""

import unittest

_strace_working = False


def requires_strace():
    return unittest.skip("strace not found")


def strace_python(code, strace_flags, check=True):
    raise unittest.SkipTest("strace not found")


def get_events(code, strace_flags, prelude="", cleanup=""):
    raise unittest.SkipTest("strace not found")


def get_syscalls(code, strace_flags, prelude="", cleanup="", ignore_memory=True):
    raise unittest.SkipTest("strace not found")
