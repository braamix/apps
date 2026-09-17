..EE...E....F....FF.E.E..E...........
======================================================================
ERROR: test_class_cause_nonexception_result (__main__.TestCause.test_class_cause_nonexception_result)
----------------------------------------------------------------------
TestCause.test_class_cause_nonexception_result.<locals>.ConstructMortal

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "/tmp/test_raise.py", line 196, in test_class_cause_nonexception_result
    raise IndexError from ConstructMortal
IndexError

======================================================================
ERROR: test_erroneous_cause (__main__.TestCause.test_erroneous_cause)
----------------------------------------------------------------------
TestCause.test_erroneous_cause.<locals>.MyException

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "/tmp/test_raise.py", line 213, in test_erroneous_cause
    raise IndexError from MyException
IndexError

======================================================================
ERROR: test_3611 (__main__.TestContext.test_3611)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_raise.py", line 491, in test_3611
    with support.catch_unraisable_exception() as cm:
AttributeError: 'module' object has no attribute 'catch_unraisable_exception'

======================================================================
ERROR: test_erroneous_exception (__main__.TestRaise.test_erroneous_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_raise.py", line 128, in test_erroneous_exception
    raise MyException
TestRaise.test_erroneous_exception.<locals>.MyException

======================================================================
ERROR: test_finally_reraise (__main__.TestRaise.test_finally_reraise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_raise.py", line 68, in test_finally_reraise
    self.assertRaises(KeyError, reraise)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_raise.py", line 62, in reraise
    raise TypeError("foo")
TypeError: foo

======================================================================
ERROR: test_new_returns_invalid_instance (__main__.TestRaise.test_new_returns_invalid_instance)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_raise.py", line 141, in test_new_returns_invalid_instance
    raise MyException
TestRaise.test_new_returns_invalid_instance.<locals>.MyException

======================================================================
FAIL: test_context_manager (__main__.TestContext.test_context_manager)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_raise.py", line 397, in test_context_manager
    with ContextManager():
  File "/tmp/test_raise.py", line 395, in __exit__
    xyzzy
NameError: name 'xyzzy' is not defined

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_raise.py", line 400, in test_context_manager
    self.assertIsInstance(e.__context__, ZeroDivisionError)
AssertionError: None is not an instance of <class 'ZeroDivisionError'>

======================================================================
FAIL: test_raise_finally (__main__.TestContext.test_raise_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_raise.py", line 384, in test_raise_finally
    raise OSError
OSError

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_raise.py", line 386, in test_raise_finally
    self.assertIsInstance(e.__context__, ZeroDivisionError)
AssertionError: None is not an instance of <class 'ZeroDivisionError'>

======================================================================
FAIL: test_reraise_cycle_broken (__main__.TestContext.test_reraise_cycle_broken)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_raise.py", line 422, in test_reraise_cycle_broken
    1/0
ZeroDivisionError: division by zero

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_raise.py", line 424, in test_reraise_cycle_broken
    raise a
  File "/tmp/test_raise.py", line 419, in test_reraise_cycle_broken
    xyzzy
NameError: name 'xyzzy' is not defined

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_raise.py", line 426, in test_reraise_cycle_broken
    self.assertIsNone(e.__context__.__context__)
AssertionError: NameError("name 'xyzzy' is not defined") is not None

----------------------------------------------------------------------
Ran 37 tests in Ns

FAILED (failures=3, errors=6)
