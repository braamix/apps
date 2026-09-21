...ssssssssss.sss...s...s....................Fs..sEE.............F
======================================================================
ERROR: test_sleep (__main__.TimeTestCase.test_sleep) (value=Decimal('0.02'))
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_time.py", line 181, in test_sleep
    time.sleep(value)
TypeError: must be real number, not Decimal

======================================================================
ERROR: test_sleep (__main__.TimeTestCase.test_sleep) (value=Fraction(1, 50))
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_time.py", line 181, in test_sleep
    time.sleep(value)
TypeError: must be real number, not Fraction

======================================================================
FAIL: test_monotonic (__main__.TimeTestCase.test_monotonic)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_time.py", line 577, in test_monotonic
    self.assertGreater(t2, t1)
AssertionError: 0.0 not greater than 0.0

======================================================================
FAIL: test_tzset (__main__.TimeTestCase.test_tzset)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_time.py", line 469, in test_tzset
    self.assertNotEqual(time.gmtime(xmas2002), time.localtime(xmas2002))
AssertionError: time.struct_time(tm_year=2002, tm_mon=12, tm_mday=25, tm_hour=0, tm_min=0, tm_sec=0, tm_wday=2, tm_yday=359, tm_isdst=0) == time.struct_time(tm_year=2002, tm_mon=12, tm_mday=25, tm_hour=0, tm_min=0, tm_sec=0, tm_wday=2, tm_yday=359, tm_isdst=0)

----------------------------------------------------------------------
Ran 65 tests in Ns

FAILED (failures=2, errors=2, skipped=17)
