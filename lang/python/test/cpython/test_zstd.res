ZSTD_VERSION = 1.6.0
zstd_version = 1.6.0
ZSTD_VERSION_INFO = _zstd.zstd_version_info(major=1, minor=6, patch=0)
zstd_version_info = _zstd.zstd_version_info(major=1, minor=6, patch=0)
....E..s.F..........E....................................................E..........ssss...............E.....F.FEF.EF.F
======================================================================
ERROR: test_set_pledged_input_size (__main__.CompressorTestCase.test_set_pledged_input_size)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 465, in test_set_pledged_input_size
    c.set_pledged_input_size(300)
_zstd.ZstdError: Only a ZstdCompressor at the start of a frame can have its pledged input size set

======================================================================
ERROR: test_simple_decompress_bad_args (__main__.DecompressorTestCase.test_simple_decompress_bad_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 553, in test_simple_decompress_bad_args
    ZstdDecompressor(options={2**1000: 100})
TypeError: key of the options dict should be a DecompressionParameter attribute: int

======================================================================
ERROR: test_write (__main__.FileTestCase.test_write)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 2115, in test_write
    f.write(raw_data)
  File "/pkg/store/python-0/lib/compression/zstd/_zstdfile.py", line 133, in write
    compressed = self._compressor.compress(data)
_zstd.ZstdError: Unable to compress zstd data: Allocation error : not enough memory

======================================================================
ERROR: test_option (__main__.OpenTestCase.test_option)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 2598, in test_option
    f.write(DECOMPRESSED_100_PLUS_32KB)
  File "/pkg/store/python-0/lib/compression/zstd/_zstdfile.py", line 133, in write
    compressed = self._compressor.compress(data)
_zstd.ZstdError: Unable to compress zstd data: Allocation error : not enough memory

======================================================================
ERROR: test_invalid_dict (__main__.ZstdDictTestCase.test_invalid_dict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1271, in test_invalid_dict
    ZstdCompressor(zstd_dict=zd.as_digested_dict)
MemoryError: out of memory

======================================================================
ERROR: test_train_buffer_protocol_samples (__main__.ZstdDictTestCase.test_train_buffer_protocol_samples)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1491, in test_train_buffer_protocol_samples
    concatenation = b''.join(chunk_lst)
TypeError: sequence item: expected a bytes-like object: array.array

======================================================================
FAIL: test_decompressor_skippable (__main__.DecompressorFlagsTestCase.test_decompressor_skippable)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1171, in test_decompressor_skippable
    self.assertTrue(d.eof)
AssertionError: False is not true

======================================================================
FAIL: test_finalize_dict (__main__.ZstdDictTestCase.test_finalize_dict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1341, in test_finalize_dict
    self.assertNotEqual(dic2.dict_id, 0)
AssertionError: 0 == 0

======================================================================
FAIL: test_finalize_dict_c (__main__.ZstdDictTestCase.test_finalize_dict_c)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1430, in test_finalize_dict_c
    with self.assertRaises(TypeError):
AssertionError: TypeError not raised

======================================================================
FAIL: test_is_raw (__main__.ZstdDictTestCase.test_is_raw)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1238, in test_is_raw
    with self.assertRaises(ValueError):
AssertionError: ValueError not raised

======================================================================
FAIL: test_train_dict (__main__.ZstdDictTestCase.test_train_dict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1314, in test_train_dict
    self.assertNotEqual(TRAINED_DICT.dict_id, 0)
AssertionError: 0 == 0

======================================================================
FAIL: test_train_dict_c (__main__.ZstdDictTestCase.test_train_dict_c)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_zstd.py", line 1391, in test_train_dict_c
    with self.assertRaises(TypeError):
AssertionError: TypeError not raised

----------------------------------------------------------------------
Ran 119 tests in Ns

FAILED (failures=6, errors=6, skipped=5)
