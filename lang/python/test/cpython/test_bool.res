...F....F...F.......F.EE.......
======================================================================
ERROR: test_pickle (__main__.BoolTest.test_pickle)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bool.py", line 290, in test_pickle
    import pickle
ModuleNotFoundError: Standard library module 'pickle' was not found

======================================================================
ERROR: test_picklevalues (__main__.BoolTest.test_picklevalues)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bool.py", line 297, in test_picklevalues
    import pickle
ModuleNotFoundError: Standard library module 'pickle' was not found

======================================================================
FAIL: test_boolean (__main__.BoolTest.test_boolean)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bool.py", line 245, in test_boolean
    self.assertIs(True & True, True)
AssertionError: 1 is not True

======================================================================
FAIL: test_convert_to_bool (__main__.BoolTest.test_convert_to_bool)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bool.py", line 313, in test_convert_to_bool
    check(Foo())
  File "/tmp/test_bool.py", line 309, in <lambda>
    check = lambda o: self.assertRaises(TypeError, bool, o)
AssertionError: TypeError not raised by bool

======================================================================
FAIL: test_from_bytes (__main__.BoolTest.test_from_bytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bool.py", line 357, in test_from_bytes
    self.assertIs(bool.from_bytes(b'\x00'*8, 'big'), False)
AssertionError: 0 is not False

======================================================================
FAIL: test_math (__main__.BoolTest.test_math)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bool.py", line 61, in test_math
    with self.assertWarns(DeprecationWarning):
AssertionError: DeprecationWarning not triggered

----------------------------------------------------------------------
Ran 31 tests in Ns

FAILED (failures=4, errors=2)
