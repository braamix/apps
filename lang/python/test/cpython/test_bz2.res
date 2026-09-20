bzlib_version = 1.0.8, 13-Jul-2019
bzlib_version_info = _bz2.bzlib_version_info(major=1, minor=0, patch=8)
.s....s..F........s......................................................s.............................
======================================================================
FAIL: testDecompressorChunksMaxsize (__main__.BZ2DecompressorTest.testDecompressorChunksMaxsize)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 948, in testDecompressorChunksMaxsize
    self.assertFalse(bzd.needs_input)
AssertionError: True is not false

----------------------------------------------------------------------
Ran 103 tests in Ns

FAILED (failures=1, skipped=4)
