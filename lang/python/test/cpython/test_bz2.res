bzlib_version = 1.0.8, 13-Jul-2019
bzlib_version_info = _bz2.bzlib_version_info(major=1, minor=0, patch=8)
.s....s...........s.............E................EEEEE.....EEEEEE........s........E.....EEEEE..E.EEE.E.
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

======================================================================
ERROR: testReadLineMultiStream (__main__.BZ2FileTest.testReadLineMultiStream)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 224, in testReadLineMultiStream
    self.assertEqual(bz2f.readline(), line)
  File "/pkg/store/python-0/lib/bz2.py", line 208, in readline
    return self._buffer.readline(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 68, in readinto
    data = self.read(len(byte_view))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 91, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testReadLines (__main__.BZ2FileTest.testReadLines)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 230, in testReadLines
    self.assertEqual(bz2f.readlines(), self.TEXT_LINES)
  File "/pkg/store/python-0/lib/bz2.py", line 222, in readlines
    return self._buffer.readlines(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 68, in readinto
    data = self.read(len(byte_view))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testReadLinesMultiStream (__main__.BZ2FileTest.testReadLinesMultiStream)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 236, in testReadLinesMultiStream
    self.assertEqual(bz2f.readlines(), self.TEXT_LINES * 5)
  File "/pkg/store/python-0/lib/bz2.py", line 222, in readlines
    return self._buffer.readlines(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 68, in readinto
    data = self.read(len(byte_view))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testReadMonkeyMultiStream (__main__.BZ2FileTest.testReadMonkeyMultiStream)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 145, in testReadMonkeyMultiStream
    self.assertEqual(bz2f.read(), self.TEXT * 5)
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testReadMultiStream (__main__.BZ2FileTest.testReadMultiStream)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 134, in testReadMultiStream
    self.assertEqual(bz2f.read(), self.TEXT * 5)
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testSeekBackwardsAcrossStreams (__main__.BZ2FileTest.testSeekBackwardsAcrossStreams)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 337, in testSeekBackwardsAcrossStreams
    readto -= len(bz2f.read(readto))
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 68, in readinto
    data = self.read(len(byte_view))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 91, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testSeekBackwardsBytesIO (__main__.BZ2FileTest.testSeekBackwardsBytesIO)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 805, in testSeekBackwardsBytesIO
    bz2f.read(500)
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 68, in readinto
    data = self.read(len(byte_view))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testSeekBackwardsFromEnd (__main__.BZ2FileTest.testSeekBackwardsFromEnd)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 344, in testSeekBackwardsFromEnd
    bz2f.seek(-150, 2)
  File "/pkg/store/python-0/lib/bz2.py", line 271, in seek
    return self._buffer.seek(offset, whence)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 139, in seek
    while self.read(io.DEFAULT_BUFFER_SIZE):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testSeekBackwardsFromEndAcrossStreams (__main__.BZ2FileTest.testSeekBackwardsFromEndAcrossStreams)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 350, in testSeekBackwardsFromEndAcrossStreams
    bz2f.seek(-1000, 2)
  File "/pkg/store/python-0/lib/bz2.py", line 271, in seek
    return self._buffer.seek(offset, whence)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 139, in seek
    while self.read(io.DEFAULT_BUFFER_SIZE):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testSeekForward (__main__.BZ2FileTest.testSeekForward)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 315, in testSeekForward
    bz2f.seek(150)
  File "/pkg/store/python-0/lib/bz2.py", line 271, in seek
    return self._buffer.seek(offset, whence)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 153, in seek
    data = self.read(min(io.DEFAULT_BUFFER_SIZE, offset))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: testSeekForwardAcrossStreams (__main__.BZ2FileTest.testSeekForwardAcrossStreams)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 322, in testSeekForwardAcrossStreams
    bz2f.seek(len(self.TEXT) + 150)
  File "/pkg/store/python-0/lib/bz2.py", line 271, in seek
    return self._buffer.seek(offset, whence)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 153, in seek
    data = self.read(min(io.DEFAULT_BUFFER_SIZE, offset))
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: test_read_truncated (__main__.BZ2FileTest.test_read_truncated)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 813, in test_read_truncated
    self.assertRaises(EOFError, f.read)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
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
ERROR: test_binary_modes (__main__.OpenTest.test_binary_modes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1124, in test_binary_modes
    file_data = ext_decompress(f.read())
  File "/tmp/test_bz2.py", line 36, in ext_decompress
    return bz2.decompress(data)
  File "/pkg/store/python-0/lib/bz2.py", line 343, in decompress
    res = decomp.decompress(data)
MemoryError: out of memory

======================================================================
ERROR: test_encoding_error_handler (__main__.OpenTest.test_encoding_error_handler)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1214, in test_encoding_error_handler
    self.assertEqual(f.read(), "foobar")
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

======================================================================
ERROR: test_fileobj (__main__.OpenTest.test_fileobj)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1176, in test_fileobj
    self.assertEqual(f.read(), self.TEXT)
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
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

======================================================================
ERROR: test_text_modes (__main__.OpenTest.test_text_modes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_bz2.py", line 1157, in test_text_modes
    self.assertEqual(f.read(), text)
  File "/pkg/store/python-0/lib/bz2.py", line 174, in read
    return self._buffer.read(size)
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 118, in readall
    while data := self.read(sys.maxsize):
  File "/pkg/store/python-0/lib/compression/_common/_streams.py", line 103, in read
    data = self._decompressor.decompress(rawblock, size)
MemoryError: out of memory

----------------------------------------------------------------------
Ran 103 tests in Ns

FAILED (errors=23, skipped=4)
