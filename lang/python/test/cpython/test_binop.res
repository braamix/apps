EFF.........
======================================================================
ERROR: test_fallback_ne_blocking (__main__.FallbackBlockingTests.test_fallback_ne_blocking)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_binop.py", line 436, in test_fallback_ne_blocking
    self.assertFalse(e != xn)
TypeError: 'NoneType' object is not callable

======================================================================
FAIL: test_fallback_rmethod_blocking (__main__.FallbackBlockingTests.test_fallback_rmethod_blocking)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_binop.py", line 428, in test_fallback_rmethod_blocking
    self.assertRaises(TypeError, eq, e, s)
AssertionError: TypeError not raised by eq

======================================================================
FAIL: test_comparison_orders (__main__.OperationOrderTests.test_comparison_orders)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_binop.py", line 377, in test_comparison_orders
    self.assertEqual(op_sequence(eq, B, C), ['C.__eq__', 'B.__eq__'])
AssertionError: Lists differ: ['B.__eq__', 'C.__eq__'] != ['C.__eq__', 'B.__eq__']

First differing element 0:
'B.__eq__'
'C.__eq__'

- ['B.__eq__', 'C.__eq__']
+ ['C.__eq__', 'B.__eq__']

----------------------------------------------------------------------
Ran 12 tests in Ns

FAILED (failures=2, errors=1)
