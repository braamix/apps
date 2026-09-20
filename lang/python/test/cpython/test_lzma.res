LZMA_VERSION = 5.8.4
lzma_version = 5.8.4
LZMA_VERSION_INFO = _lzma.lzma_version_info(major=5, minor=8, patch=4, stability='stable')
lzma_version_info = _lzma.lzma_version_info(major=5, minor=8, patch=4, stability='stable')
E..........s....s................s...........E.....................................................E........................
======================================================================
ERROR: test_bad_args (__main__.CompressDecompressFunctionTestCase.test_bad_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_lzma.py", line 413, in test_bad_args
    lzma.decompress(b"", format=lzma.FORMAT_RAW, filters={})
  File "/pkg/store/python-0/lib/lzma.py", line 351, in decompress
    decomp = LZMADecompressor(format, memlimit, filters)
ValueError: Empty filter chain

======================================================================
ERROR: test_decompress_limited (__main__.FileTestCase.test_decompress_limited)
Decompressed data buffering should be limited
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_lzma.py", line 1076, in test_decompress_limited
    bomb = lzma.compress(b'\0' * int(2e6), preset=6)
  File "/pkg/store/python-0/lib/lzma.py", line 337, in compress
    comp = LZMACompressor(format, check, preset, filters)
MemoryError: LZMACompressor wanted 93 MiB and the process had none to give: use a lower preset

======================================================================
ERROR: test_write (__main__.FileTestCase.test_write)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_lzma.py", line 1104, in test_write
    with LZMAFile(dst, "w", format=lzma.FORMAT_RAW,
  File "/pkg/store/python-0/lib/lzma.py", line 113, in __init__
    self._compressor = LZMACompressor(format=format, check=check,
MemoryError: LZMACompressor wanted 93 MiB and the process had none to give: use a lower preset

----------------------------------------------------------------------
Ran 124 tests in Ns

FAILED (errors=3, skipped=3)
