"""test.support.script_helper: there is no subprocess, so a test that runs a
second interpreter skips."""

import unittest


def _skip(*args, **kwargs):
    raise unittest.SkipTest("no subprocess")


assert_python_ok = _skip
assert_python_failure = _skip
spawn_python = _skip
run_python_until_end = _skip
kill_python = _skip
make_script = _skip
make_zip_script = _skip
make_pkg = _skip
make_zip_pkg = _skip
run_test_script = _skip


def interpreter_requires_environment():
    return False
