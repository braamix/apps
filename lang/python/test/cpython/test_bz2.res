bzlib_version = 1.0.8, 13-Jul-2019
bzlib_version_info = _bz2.bzlib_version_info(major=1, minor=0, patch=8)
.s....s...........s.............E........................................s.............................
======================================================================
ERROR: testOpenFileWithIntName (__main__.BZ2FileTest.testOpenFileWithIntName)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 721, in testOpenFileWithIntName
    self.assertEqual(f.read(), b'contentappendix')
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 91, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

----------------------------------------------------------------------
Ran 103 tests in Ns

FAILED (errors=1, skipped=4)
