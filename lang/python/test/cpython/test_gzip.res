..................................E.............s..EE..........E..E............
======================================================================
ERROR: test_issue44439 (__main__.TestGzip.test_issue44439)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_gzip.py", line 893, in test_issue44439
    self.assertEqual(f.write(q), LENGTH)
  File "/pkg/store/python-0/lib/gzip.py", line 325, in write
    return self._buffer.write(data)
TypeError: a bytes-like object is required, not 'array.array'

======================================================================
ERROR: test_readinto (__main__.TestGzip.test_readinto)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_gzip.py", line 151, in test_readinto
    f.write(large_data)
  File "/pkg/store/python-0/lib/gzip.py", line 325, in write
    return self._buffer.write(data)
MemoryError: out of memory

======================================================================
ERROR: test_readinto1 (__main__.TestGzip.test_readinto1)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_gzip.py", line 161, in test_readinto1
    large_data = os.urandom(10 * 2**20)
MemoryError: out of memory

======================================================================
ERROR: test_write_array (__main__.TestGzip.test_write_array)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_gzip.py", line 110, in test_write_array
    self.write_and_read_back(array.array('I', data1 * 40))
  File "/tmp/test_gzip.py", line 62, in write_and_read_back
    b_data = bytes(data)
ValueError: bytes must be in range(0, 256)

======================================================================
ERROR: test_write_memoryview (__main__.TestGzip.test_write_memoryview)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_gzip.py", line 103, in test_write_memoryview
    data = m.cast('B', shape=[8,8,4])
TypeError: cast() takes no keyword arguments

----------------------------------------------------------------------
Ran 79 tests in Ns

FAILED (errors=5, skipped=1)
