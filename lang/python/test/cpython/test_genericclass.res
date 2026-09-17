s...F.................
======================================================================
FAIL: test_class_getitem_errors_2 (__main__.TestClassGetitem.test_class_getitem_errors_2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_genericclass.py", line 251, in test_class_getitem_errors_2
    with self.assertRaisesRegex(TypeError, "C_is_none"):
AssertionError: "C_is_none" does not match "'NoneType' object is not callable"

----------------------------------------------------------------------
Ran 22 tests in Ns

FAILED (failures=1, skipped=1)
