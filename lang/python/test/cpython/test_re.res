sssss..ssss.s.s......................................................................................F.ssss.......s.........E...................s....s....................
======================================================================
ERROR: test_pickling (__main__.ReTests.test_pickling)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_re.py", line 1632, in test_pickling
    import pickle
ModuleNotFoundError: Standard library module 'pickle' was not found

======================================================================
FAIL: test_keep_buffer (__main__.ReTests.test_keep_buffer)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_re.py", line 73, in test_keep_buffer
    with self.assertRaises(BufferError):
AssertionError: BufferError not raised

----------------------------------------------------------------------
Ran 170 tests in Ns

FAILED (failures=1, errors=1, skipped=18)
