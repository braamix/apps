.F
======================================================================
FAIL: test_stopiteration_wrapping_context (__main__.TestPEP479.test_stopiteration_wrapping_context)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_generator_stop.py", line 20, in g
    yield f()
  File "/tmp/test_generator_stop.py", line 18, in f
    raise StopIteration
StopIteration

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_generator_stop.py", line 23, in test_stopiteration_wrapping_context
    next(g())
RuntimeError: generator raised StopIteration

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_generator_stop.py", line 25, in test_stopiteration_wrapping_context
    self.assertIs(type(exc.__cause__), StopIteration)
AssertionError: <class 'NoneType'> is not <class 'StopIteration'>

----------------------------------------------------------------------
Ran 2 tests in Ns

FAILED (failures=1)
