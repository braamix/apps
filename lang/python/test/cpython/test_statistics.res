.......................................F..F........................................sE..............................F..s.s.E.......................E.........................................................................................s..sssssssssssssssssss....F.s..................................................E........s............................................E..............................
======================================================================
ERROR: test_basics (__main__.TestFMean.test_basics)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 1883, in test_basics
    actual_mean = fmean(data)
  File "/pkg/store/python-0/lib/statistics.py", line 203, in fmean
    total = fsum(data)
TypeError: must be real number, not Decimal

======================================================================
ERROR: test_float_output (__main__.TestLinearRegression.test_float_output)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 2872, in test_float_output
    slope, intercept = statistics.linear_regression(x, y)
  File "/pkg/store/python-0/lib/statistics.py", line 792, in linear_regression
    xbar = fsum(x) / n
TypeError: must be real number, not Fraction

======================================================================
ERROR: test_type_of_data_element (__main__.TestMean.test_type_of_data_element)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 1124, in test_type_of_data_element
    result = type(expected)(self.func(data))
TypeError: float() argument must be a number or a string: MyFloat

======================================================================
ERROR: test_type_of_data_element (__main__.TestPVariance.test_type_of_data_element)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 1124, in test_type_of_data_element
    result = type(expected)(self.func(data))
TypeError: float() argument must be a number or a string: MyFloat

======================================================================
ERROR: test_type_of_data_element (__main__.TestVariance.test_type_of_data_element)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 1124, in test_type_of_data_element
    result = type(expected)(self.func(data))
TypeError: float() argument must be a number or a string: MyFloat

======================================================================
FAIL: test_float (__main__.ConvertTest.test_float)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 971, in test_float
    self.check_exact_equal(x, MyFloat(1.125))
  File "/tmp/test_statistics.py", line 942, in check_exact_equal
    self.assertEqual(x, y)
AssertionError: <continuation> != 1.125

======================================================================
FAIL: test_int (__main__.ConvertTest.test_int)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 951, in test_int
    self.check_exact_equal(x, MyInt(17))
  File "/tmp/test_statistics.py", line 942, in check_exact_equal
    self.assertEqual(x, y)
AssertionError: <continuation> != 17

======================================================================
FAIL: test_types_conserved (__main__.TestHarmonicMean.test_types_conserved)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 1167, in test_types_conserved
    self.assertIs(type(result), kind)
AssertionError: <class 'float'> is not <class '__main__.UnivariateTypeMixin.prepare_types_for_conservation_test.<locals>.MyFloat'>

======================================================================
FAIL: test_hashability (__main__.TestNormalDistPython.test_hashability)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_statistics.py", line 3290, in test_hashability
    self.assertEqual(len(s), 3)
AssertionError: 5 != 3

----------------------------------------------------------------------
Ran 400 tests in Ns

FAILED (failures=4, errors=5, skipped=25)
