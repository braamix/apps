.........EE
======================================================================
ERROR: test_scriptdecode (__main__.QuopriTestCase.test_scriptdecode)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_quopri.py", line 207, in test_scriptdecode
    cout, cerr = process.communicate(e)
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
ERROR: test_scriptencode (__main__.QuopriTestCase.test_scriptencode)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_quopri.py", line 190, in test_scriptencode
    cout, cerr = process.communicate(p)
  File "/pkg/store/python-0/lib/subprocess.py", line 1395, in communicate
    stdout, stderr = self._communicate(input, endtime, timeout)
  File "/pkg/store/python-0/lib/subprocess.py", line 2413, in _communicate
    new_offset, completed = _communicate_io_posix(
  File "/pkg/store/python-0/lib/subprocess.py", line 291, in _communicate_io_posix
    ready = selector.select(remaining)
  File "/pkg/store/python-0/lib/selectors.py", line 314, in select
    r, w, _ = self._select(self._readers, self._writers, [], timeout)
OSError: [Errno 95] Operation not supported

----------------------------------------------------------------------
Ran 11 tests in Ns

FAILED (errors=2)
