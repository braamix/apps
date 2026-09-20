/pkg/store/python-0/lib/unittest/case.py:747: DeprecationWarning: It is deprecated to return a value that is not None from a test case (<bound method <function <lambda>.<locals>.<lambda>>> returned 'FMATests')
  return self.run(*args, **kwds)
.....F.........................E......E../pkg/store/python-0/lib/unittest/case.py:747: DeprecationWarning: It is deprecated to return a value that is not None from a test case (<bound method <function <lambda>.<locals>.<lambda>>> returned 'MathTests')
  return self.run(*args, **kwds)
........EE.E....FFFFFFFFEEEEEEEEE...E...........EFs.E....sssEF.F.
======================================================================
ERROR: testDist (__main__.MathTests.testDist)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1009, in testDist
    self.assertEqual(dist((FloatLike(14.), 1), (2, -4)), 13)
TypeError: must be real number, not FloatLike

======================================================================
ERROR: testFsum (__main__.MathTests.testFsum)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 780, in testFsum
    self.assertEqual(math.fsum([1e100, FloatLike(1.0), -1e100, 1e-100,
TypeError: must be real number, not FloatLike

======================================================================
ERROR: testLog (__main__.MathTests.testLog)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1231, in testLog
    self.assertAlmostEqual(math.log(IndexableFloatLike(OverflowError(), 10**1000)),
  File "/tmp/test_math.py", line 195, in __float__
    raise self.float_value
OverflowError

======================================================================
ERROR: testLog10 (__main__.MathTests.testLog10)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1303, in testLog10
    self.assertEqual(math.log10(IndexableFloatLike(OverflowError(), 10**1000)), 1000.0)
  File "/tmp/test_math.py", line 195, in __float__
    raise self.float_value
OverflowError

======================================================================
ERROR: testLog2 (__main__.MathTests.testLog2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1263, in testLog2
    self.assertEqual(math.log2(IndexableFloatLike(OverflowError(), 2**2000)), 2000.0)
  File "/tmp/test_math.py", line 195, in __float__
    raise self.float_value
OverflowError

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case=' 1  0.c  0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case='-1  0.c -0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case=' 1 -0.c  0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case='-1 -0.c -0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case=' 1.4  0.c -0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case='-1.4  0.c  0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case=' 1.4 -0.c -0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder) (case='-1.4 -0.c  0.4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1839, in testRemainder
    y = float.fromhex(y_hex)
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testRemainder (__main__.MathTests.testRemainder)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1850, in testRemainder
    tiny = float.fromhex('1p-1074')  # min +ve subnormal
ValueError: invalid hexadecimal floating-point string

======================================================================
ERROR: testSumProd (__main__.MathTests.testSumProd)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1372, in testSumProd
    from test.test_iter import BasicIterClass
ModuleNotFoundError: No module named 'test.test_iter'. Did you mean: 'test.testcodec'?

======================================================================
ERROR: test_issue39871 (__main__.MathTests.test_issue39871)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 2408, in test_issue39871
    func("not a number", y)
  File "/tmp/test_math.py", line 2404, in __float__
    1/0
ZeroDivisionError: division by zero

======================================================================
ERROR: test_mtestfile (__main__.MathTests.test_mtestfile)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 2148, in test_mtestfile
    for id, fn, args, expected, flags in parse_mtestfile(math_testcases):
  File "/tmp/test_math.py", line 81, in parse_mtestfile
    with open(fname, encoding="utf-8") as fp:
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/mathdata/math_testcases.txt'

======================================================================
ERROR: test_testfile (__main__.MathTests.test_testfile)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 2095, in test_testfile
    for id, fn, ar, ai, er, ei, flags in parse_testfile(test_file):
  File "/tmp/test_math.py", line 104, in parse_testfile
    with open(fname, encoding="utf-8") as fp:
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/mathdata/cmath_testcases.txt'

======================================================================
FAIL: test_fma_zero_result (__main__.FMATests.test_fma_zero_result)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 2725, in test_fma_zero_result
    self.assertIsNegativeZero(math.fma(tiny, -tiny, 0.0))
  File "/tmp/test_math.py", line 2846, in assertIsNegativeZero
    self.assertTrue(
AssertionError: False is not true : Expected a negative zero, got 0.0

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case='-3.8 1  0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case='-2.8 1 -0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case='-1.8 1  0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case='-0.8 1 -0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case=' 0.8 1  0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case=' 1.8 1 -0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case=' 2.8 1  0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: testRemainder (__main__.MathTests.testRemainder) (case=' 3.8 1 -0.8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1841, in testRemainder
    validate_spec(x, y, expected)
  File "/tmp/test_math.py", line 1767, in validate_spec
    self.assertLessEqual(abs(fr), abs(fy/2))
AssertionError: Fraction(3602879701896397, 4503599627370496) not less than or equal to Fraction(1, 2)

======================================================================
FAIL: test_lcm (__main__.MathTests.test_lcm)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1155, in test_lcm
    self.assertRaises(TypeError, lcm, 120, 0, 84.0)
AssertionError: TypeError not raised by lcm

======================================================================
FAIL: test_trunc (__main__.MathTests.test_trunc)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_math.py", line 1978, in test_trunc
    self.assertRaises(TypeError, math.trunc, FloatLike(23.5))
AssertionError: TypeError not raised by trunc

======================================================================
FAIL: /tmp/mathdata/ieee754.txt [38]
Doctest: ieee754.txt
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/mathdata/ieee754.txt", line 116, in ieee754.txt
    >>> 0 ** -1
AssertionError: Failed example:
    0 ** -1
Expected:
    Traceback (most recent call last):
    ...
    ZeroDivisionError: zero to a negative power
Got:
    inf

----------------------------------------------------------------------
Ran 89 tests in Ns

FAILED (failures=12, errors=18, skipped=4)
