.......................................................s..F.F.sssss
======================================================================
FAIL: test_decodebytes (__main__.LegacyBase64TestCase.test_decodebytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_base64.py", line 73, in test_decodebytes
    self.check_type_errors(base64.decodebytes)
  File "/tmp/test_base64.py", line 29, in check_type_errors
    self.assertRaises(TypeError, f, multidimensional)
AssertionError: TypeError not raised by decodebytes

======================================================================
FAIL: test_encodebytes (__main__.LegacyBase64TestCase.test_encodebytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_base64.py", line 52, in test_encodebytes
    self.check_type_errors(base64.encodebytes)
  File "/tmp/test_base64.py", line 29, in check_type_errors
    self.assertRaises(TypeError, f, multidimensional)
AssertionError: TypeError not raised by encodebytes

----------------------------------------------------------------------
Ran 67 tests in Ns

FAILED (failures=2, skipped=6)
