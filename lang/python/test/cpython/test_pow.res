.....FF
======================================================================
FAIL: test_powfloat (__main__.PowTest.test_powfloat)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pow.py", line 63, in test_powfloat
    self.powtest(float)
  File "/tmp/test_pow.py", line 34, in powtest
    self.assertRaises(ZeroDivisionError, pow, zero, exp)
AssertionError: ZeroDivisionError not raised by pow

======================================================================
FAIL: test_powint (__main__.PowTest.test_powint)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pow.py", line 60, in test_powint
    self.powtest(int)
  File "/tmp/test_pow.py", line 34, in powtest
    self.assertRaises(ZeroDivisionError, pow, zero, exp)
AssertionError: ZeroDivisionError not raised by pow

----------------------------------------------------------------------
Ran 7 tests in Ns

FAILED (failures=2)
