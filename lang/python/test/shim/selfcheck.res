...................FEFx.ss.............ss.ss..s..
======================================================================
ERROR: test_this_one_errors (__main__.Outcomes.test_this_one_errors)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/selfcheck.py", line 201, in test_this_one_errors
    raise KeyError("not an assertion")
KeyError: 'not an assertion'

======================================================================
FAIL: test_subtest_failure_is_reported (__main__.Outcomes.test_subtest_failure_is_reported) (i=2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/selfcheck.py", line 209, in test_subtest_failure_is_reported
    self.assertEqual(i, 1)
AssertionError: 2 != 1

======================================================================
FAIL: test_this_one_fails (__main__.Outcomes.test_this_one_fails)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/selfcheck.py", line 198, in test_this_one_fails
    self.assertEqual(1, 2)
AssertionError: 1 != 2

----------------------------------------------------------------------
Ran 49 tests in Ns

FAILED (failures=2, errors=1, skipped=7, expected failures=1)
