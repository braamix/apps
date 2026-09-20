bzlib_version = 1.0.8, 13-Jul-2019
bzlib_version_info = _bz2.bzlib_version_info(major=1, minor=0, patch=8)
.s....s...........s..................EEE.................................s..............EEEEE......E...
======================================================================
ERROR: testOpenPathLikeFilename (__main__.BZ2FileTest.testOpenPathLikeFilename)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 749, in testOpenPathLikeFilename
    self.assertEqual(f.read(), self.DATA)
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testPeek (__main__.BZ2FileTest.testPeek)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 195, in testPeek
    pdata = bz2f.peek()
  File "/pkg/store/python-0/lib/bz2.py", line 165, in peek
    return self._buffer.peek(n)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 68, in readinto
    data = self.read(len(byte_view))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testPeekBytesIO (__main__.BZ2FileTest.testPeekBytesIO)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 779, in testPeekBytesIO
    pdata = bz2f.peek()
  File "/pkg/store/python-0/lib/bz2.py", line 165, in peek
    return self._buffer.peek(n)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 68, in readinto
    data = self.read(len(byte_view))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testDecompressIncomplete (__main__.CompressDecompressTest.testDecompressIncomplete)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1086, in testDecompressIncomplete
    self.assertRaises(ValueError, bz2.decompress, self.DATA[:-10])
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/bz2.py", line 343, in decompress
    res = decomp.decompress(data)
MemoryError: out of memory

======================================================================
ERROR: testDecompressMultiStream (__main__.CompressDecompressTest.testDecompressMultiStream)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1092, in testDecompressMultiStream
    text = bz2.decompress(self.DATA * 5)
  File "/pkg/store/python-0/lib/bz2.py", line 343, in decompress
    res = decomp.decompress(data)
MemoryError: out of memory

======================================================================
ERROR: testDecompressMultiStreamTrailingJunk (__main__.CompressDecompressTest.testDecompressMultiStreamTrailingJunk)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1100, in testDecompressMultiStreamTrailingJunk
    text = bz2.decompress(self.DATA * 5 + self.BAD_DATA)
  File "/pkg/store/python-0/lib/bz2.py", line 343, in decompress
    res = decomp.decompress(data)
MemoryError: out of memory

======================================================================
ERROR: testDecompressToEmptyString (__main__.CompressDecompressTest.testDecompressToEmptyString)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1082, in testDecompressToEmptyString
    text = bz2.decompress(self.EMPTY_DATA)
  File "/pkg/store/python-0/lib/bz2.py", line 343, in decompress
    res = decomp.decompress(data)
MemoryError: out of memory

======================================================================
ERROR: testDecompressTrailingJunk (__main__.CompressDecompressTest.testDecompressTrailingJunk)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1096, in testDecompressTrailingJunk
    text = bz2.decompress(self.DATA + self.BAD_DATA)
  File "/pkg/store/python-0/lib/bz2.py", line 343, in decompress
    res = decomp.decompress(data)
MemoryError: out of memory

======================================================================
ERROR: test_implicit_binary_modes (__main__.OpenTest.test_implicit_binary_modes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1142, in test_implicit_binary_modes
    file_data = ext_decompress(f.read())
  File "/tmp/test_bz2.py", line 36, in ext_decompress
    return bz2.decompress(data)
  File "/pkg/store/python-0/lib/bz2.py", line 343, in decompress
    res = decomp.decompress(data)
MemoryError: out of memory

----------------------------------------------------------------------
Ran 103 tests in Ns

FAILED (errors=9, skipped=4)
