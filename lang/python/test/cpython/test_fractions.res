..E..................E.........E.......E....E....F
======================================================================
ERROR: testArithmetic (__main__.FractionTest.testArithmetic)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fractions.py", line 779, in testArithmetic
    z = pow(F(-1), F(1, 2))
TypeError: unsupported operand type(s) for **: 'Fraction' and 'Fraction'

======================================================================
ERROR: testIntGuaranteesIntReturn (__main__.FractionTest.testIntGuaranteesIntReturn)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fractions.py", line 710, in testIntGuaranteesIntReturn
    f = F(CustomInt(13), CustomInt(5))
  File "/pkg/store/python-0/lib/fractions.py", line 304, in __new__
    g = math.gcd(numerator, denominator)
TypeError: 'CustomInt' object cannot be interpreted as an integer

======================================================================
ERROR: testMixedPower (__main__.FractionTest.testMixedPower)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fractions.py", line 955, in testMixedPower
    z = pow(-1, F(1, 2))
TypeError: unsupported operand type(s) for **: 'int' and 'Fraction'

======================================================================
ERROR: test_float_format_testfile (__main__.FractionTest.test_float_format_testfile)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fractions.py", line 1650, in test_float_format_testfile
    with open(format_testfile, encoding="utf-8") as testfile:
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/mathdata/formatfloat_testcases.txt'

======================================================================
ERROR: test_int_subclass (__main__.FractionTest.test_int_subclass)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fractions.py", line 1218, in test_int_subclass
    f = fractions.Fraction(myint(1 * 3), myint(2 * 3))
  File "/pkg/store/python-0/lib/fractions.py", line 304, in __new__
    g = math.gcd(numerator, denominator)
TypeError: 'myint' object cannot be interpreted as an integer

======================================================================
FAIL: test_three_argument_pow (__main__.FractionTest.test_three_argument_pow)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fractions.py", line 309, in assertRaisesMessage
    callable(*args, **kwargs)
TypeError: pow() 3rd argument not allowed unless all arguments are ints

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_fractions.py", line 1701, in test_three_argument_pow
    self.assertRaisesMessage(TypeError,
  File "/tmp/test_fractions.py", line 311, in assertRaisesMessage
    self.assertEqual(message, str(e))
AssertionError: "unsupported operand type(s) for ** or pow(): 'Fraction', 'int', 'int'" != 'pow() 3rd argument not allowed unless all arguments are ints'
- unsupported operand type(s) for ** or pow(): 'Fraction', 'int', 'int'
+ pow() 3rd argument not allowed unless all arguments are ints


----------------------------------------------------------------------
Ran 50 tests in Ns

FAILED (failures=1, errors=5)
