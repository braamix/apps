.F.E..FF..E
======================================================================
ERROR: test_goodentry (__main__.ListTest.test_goodentry)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_richcmp.py", line 346, in test_goodentry
    self.assertIs(op(x, y), True)
  File "/tmp/test_richcmp.py", line 82, in <lambda>
    "lt": (lambda a,b: a< b, operator.lt, operator.__lt__),
TypeError: '<' not supported between instances of 'Good' and 'Good'

======================================================================
ERROR: test_mixed (__main__.VectorTest.test_mixed)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_richcmp.py", line 120, in test_mixed
    self.checkequal("lt", a, b, [True,  True,  False, False, False])
  File "/tmp/test_richcmp.py", line 100, in checkequal
    self.assertEqual(len(realres), len(expres))
TypeError: object of this type has no len(): bool

======================================================================
FAIL: test_badentry (__main__.ListTest.test_badentry)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_richcmp.py", line 333, in test_badentry
    self.assertRaises(Exc, op, x, y)
AssertionError: Exc not raised by eq

======================================================================
FAIL: test_not (__main__.MiscTest.test_not)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_richcmp.py", line 215, in test_not
    self.assertRaises(Exc, func, Bad())
AssertionError: Exc not raised by not_

======================================================================
FAIL: test_recursion (__main__.MiscTest.test_recursion)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 387, in wrapper
    return fn(*args, **kwds)
  File "/tmp/test_richcmp.py", line 227, in test_recursion
    self.assertRaises(RecursionError, operator.ne, a, b)
AssertionError: RecursionError not raised by ne

----------------------------------------------------------------------
Ran 11 tests in Ns

FAILED (failures=3, errors=2)
