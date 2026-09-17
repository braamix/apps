..................E....E.......Es.......s.....F.....................E....E.......Es.................
======================================================================
ERROR: testOpenDirFD (__main__.CAutoFileTests.testOpenDirFD)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fileio.py", line 264, in testOpenDirFD
    fd = os.open('.', os.O_RDONLY)
IsADirectoryError: [Errno 21] Is a directory: '.'

======================================================================
ERROR: testReprNoCloseFD (__main__.CAutoFileTests.testReprNoCloseFD)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fileio.py", line 198, in testReprNoCloseFD
    fd = os.open(TESTFN, os.O_RDONLY)
PermissionError: [Errno 13] Permission denied: '@test_N_tmpæ'

======================================================================
ERROR: test_subclass_repr (__main__.CAutoFileTests.test_subclass_repr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fileio.py", line 191, in test_subclass_repr
    f = TestSubclass(TESTFN)
PermissionError: [Errno 13] Permission denied: '@test_N_tmpæ'

======================================================================
ERROR: testOpenDirFD (__main__.PyAutoFileTests.testOpenDirFD)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fileio.py", line 264, in testOpenDirFD
    fd = os.open('.', os.O_RDONLY)
IsADirectoryError: [Errno 21] Is a directory: '.'

======================================================================
ERROR: testReprNoCloseFD (__main__.PyAutoFileTests.testReprNoCloseFD)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fileio.py", line 198, in testReprNoCloseFD
    fd = os.open(TESTFN, os.O_RDONLY)
PermissionError: [Errno 13] Permission denied: '@test_N_tmpæ'

======================================================================
ERROR: test_subclass_repr (__main__.PyAutoFileTests.test_subclass_repr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fileio.py", line 191, in test_subclass_repr
    f = TestSubclass(TESTFN)
  File "/pkg/store/python-0/lib/_pyio.py", line 1616, in __init__
    fd = os.open(file, flags, 0o666)
PermissionError: [Errno 13] Permission denied: '@test_N_tmpæ'

======================================================================
FAIL: testUnclosedFDOnException (__main__.COtherFileTests.testUnclosedFDOnException)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_fileio.py", line 751, in testUnclosedFDOnException
    self.assertRaises(MyException, MyFileIO, fd)
AssertionError: MyException not raised by MyFileIO

----------------------------------------------------------------------
Ran 100 tests in Ns

FAILED (failures=1, errors=6, skipped=3)
