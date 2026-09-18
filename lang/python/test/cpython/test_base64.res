.......................................................s..F.F...E..
======================================================================
ERROR: test_encode_from_stdin (__main__.TestMain.test_encode_from_stdin)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_base64.py", line 1430, in test_encode_from_stdin
    out, err = proc.communicate(b'a\xffb\n')
  File "/pkg/store/python-0/lib/subprocess.py", line 1395, in communicate
    stdout, stderr = self._communicate(input, endtime, timeout)
  File "/pkg/store/python-0/lib/subprocess.py", line 2413, in _communicate
    new_offset, completed = _communicate_io_posix(
  File "/pkg/store/python-0/lib/subprocess.py", line 291, in _communicate_io_posix
    ready = selector.select(remaining)
  File "/pkg/store/python-0/lib/selectors.py", line 314, in select
    r, w, _ = self._select(self._readers, self._writers, [], timeout)
OSError: [Errno 95] Operation not supported

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

FAILED (failures=2, errors=1, skipped=1)
