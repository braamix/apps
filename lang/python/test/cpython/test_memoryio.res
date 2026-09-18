.....Essss...FEFFE.......s.EEE..E...Fs..E..EEEF.EE....................................EEE...F...E..E...........FE.FE......s.....E.........EEE.EE................................................
======================================================================
ERROR: test_bytes_array (__main__.CBytesIOTest.test_bytes_array)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 683, in test_bytes_array
    memio = self.ioclass(a)
TypeError: a bytes-like object is required, not 'array.array'

======================================================================
ERROR: test_getbuffer_del (__main__.CBytesIOTest.test_getbuffer_del)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 495, in test_getbuffer_del
    with support.catch_unraisable_exception() as cm:
AttributeError: module 'test.support' has no attribute 'catch_unraisable_exception'

======================================================================
ERROR: test_getbuffer_gc_collect (__main__.CBytesIOTest.test_getbuffer_gc_collect)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 525, in test_getbuffer_gc_collect
    with support.catch_unraisable_exception() as cm:
AttributeError: module 'test.support' has no attribute 'catch_unraisable_exception'

======================================================================
ERROR: test_peek (__main__.CBytesIOTest.test_peek)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 603, in test_peek
    self.assertEqual(memio.peek(IntLike(3)), buf[:3])
TypeError: argument should be integer or None, not 'IntLike'

======================================================================
ERROR: test_pickling (__main__.CBytesIOTest.test_pickling)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 432, in test_pickling
    self.assertEqual(obj.foo, obj2.foo)
AttributeError: '_io.BytesIO' object has no attribute 'foo'

======================================================================
ERROR: test_read (__main__.CBytesIOTest.test_read)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 177, in test_read
    self.assertEqual(memio.read(IntLike(0)), self.EOF)
TypeError: argument should be integer or None, not 'IntLike'

======================================================================
ERROR: test_readline (__main__.CBytesIOTest.test_readline)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 210, in test_readline
    self.assertEqual(memio.readline(IntLike(0)), self.EOF)
TypeError: argument should be integer or None, not 'IntLike'

======================================================================
ERROR: test_truncate (__main__.CBytesIOTest.test_truncate)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 135, in test_truncate
    self.assertRaises(ValueError, memio.truncate, IntLike(-1))
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: an integer is required: IntLike

======================================================================
ERROR: test_write_concurrent_close (__main__.CBytesIOTest.test_write_concurrent_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 700, in test_write_concurrent_close
    self.assertRaises(ValueError, memio.write, B())
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: a bytes-like object is required, not 'B'

======================================================================
ERROR: test_write_concurrent_export (__main__.CBytesIOTest.test_write_concurrent_export)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 723, in test_write_concurrent_export
    self.assertRaises(BufferError, memio.write, B())
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: a bytes-like object is required, not 'B'

======================================================================
ERROR: test_write_mutating_buffer (__main__.CBytesIOTest.test_write_mutating_buffer)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 750, in test_write_mutating_buffer
    n = memio.write(b)
TypeError: a bytes-like object is required, not 'B'

======================================================================
ERROR: test_writelines_concurrent_close (__main__.CBytesIOTest.test_writelines_concurrent_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 713, in test_writelines_concurrent_close
    self.assertRaises(ValueError, memio.writelines, [B()])
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: a bytes-like object is required, not 'B'

======================================================================
ERROR: test_writelines_concurrent_export (__main__.CBytesIOTest.test_writelines_concurrent_export)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 733, in test_writelines_concurrent_export
    self.assertRaises(BufferError, memio.writelines, [B()])
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: a bytes-like object is required, not 'B'

======================================================================
ERROR: test_pickling (__main__.CStringIOTest.test_pickling)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 432, in test_pickling
    self.assertEqual(obj.foo, obj2.foo)
AttributeError: '_io.StringIO' object has no attribute 'foo'

======================================================================
ERROR: test_read (__main__.CStringIOTest.test_read)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 177, in test_read
    self.assertEqual(memio.read(IntLike(0)), self.EOF)
TypeError: argument should be integer or None, not 'IntLike'

======================================================================
ERROR: test_readline (__main__.CStringIOTest.test_readline)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 210, in test_readline
    self.assertEqual(memio.readline(IntLike(0)), self.EOF)
TypeError: argument should be integer or None, not 'IntLike'

======================================================================
ERROR: test_truncate (__main__.CStringIOTest.test_truncate)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 135, in test_truncate
    self.assertRaises(ValueError, memio.truncate, IntLike(-1))
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: an integer is required: IntLike

======================================================================
ERROR: test_write_str_subclass (__main__.CStringIOTest.test_write_str_subclass)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 1128, in test_write_str_subclass
    memio.write(s)
TypeError: string argument expected, got 'MyStr'

======================================================================
ERROR: test_getbuffer_del (__main__.PyBytesIOTest.test_getbuffer_del)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 495, in test_getbuffer_del
    with support.catch_unraisable_exception() as cm:
AttributeError: module 'test.support' has no attribute 'catch_unraisable_exception'

======================================================================
ERROR: test_getbuffer_gc_collect (__main__.PyBytesIOTest.test_getbuffer_gc_collect)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 525, in test_getbuffer_gc_collect
    with support.catch_unraisable_exception() as cm:
AttributeError: module 'test.support' has no attribute 'catch_unraisable_exception'

======================================================================
ERROR: test_readinto (__main__.PyBytesIOTest.test_readinto)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 556, in test_readinto
    self.assertEqual(memio.readinto(b), 0)
  File "/pkg/store/python-0/lib/_pyio.py", line 714, in readinto
    return self._readinto(b, read1=False)
  File "/pkg/store/python-0/lib/_pyio.py", line 738, in _readinto
    b[:n] = data
TypeError: cannot modify read-only memory

======================================================================
ERROR: test_write_concurrent_close (__main__.PyBytesIOTest.test_write_concurrent_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 700, in test_write_concurrent_close
    self.assertRaises(ValueError, memio.write, B())
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/_pyio.py", line 958, in write
    with memoryview(b) as view:
TypeError: memoryview: a bytes-like object is required: B

======================================================================
ERROR: test_write_concurrent_export (__main__.PyBytesIOTest.test_write_concurrent_export)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 723, in test_write_concurrent_export
    self.assertRaises(BufferError, memio.write, B())
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/_pyio.py", line 958, in write
    with memoryview(b) as view:
TypeError: memoryview: a bytes-like object is required: B

======================================================================
ERROR: test_write_mutating_buffer (__main__.PyBytesIOTest.test_write_mutating_buffer)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 750, in test_write_mutating_buffer
    n = memio.write(b)
  File "/pkg/store/python-0/lib/_pyio.py", line 958, in write
    with memoryview(b) as view:
TypeError: memoryview: a bytes-like object is required: B

======================================================================
ERROR: test_writelines_concurrent_close (__main__.PyBytesIOTest.test_writelines_concurrent_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 713, in test_writelines_concurrent_close
    self.assertRaises(ValueError, memio.writelines, [B()])
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/_pyio.py", line 591, in writelines
    self.write(line)
  File "/pkg/store/python-0/lib/_pyio.py", line 958, in write
    with memoryview(b) as view:
TypeError: memoryview: a bytes-like object is required: B

======================================================================
ERROR: test_writelines_concurrent_export (__main__.PyBytesIOTest.test_writelines_concurrent_export)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 733, in test_writelines_concurrent_export
    self.assertRaises(BufferError, memio.writelines, [B()])
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/_pyio.py", line 591, in writelines
    self.write(line)
  File "/pkg/store/python-0/lib/_pyio.py", line 958, in write
    with memoryview(b) as view:
TypeError: memoryview: a bytes-like object is required: B

======================================================================
FAIL: test_getbuffer (__main__.CBytesIOTest.test_getbuffer)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 459, in test_getbuffer
    self.assertRaises(BufferError, memio.write, b'x' * 100)
AssertionError: BufferError not raised by write

======================================================================
FAIL: test_getbuffer_delete (__main__.CBytesIOTest.test_getbuffer_delete)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 481, in test_getbuffer_delete
    self.assertEqual(bytes(buf), b"1234567890")
AssertionError: b'' != b'1234567890'

======================================================================
FAIL: test_getbuffer_empty (__main__.CBytesIOTest.test_getbuffer_empty)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 507, in test_getbuffer_empty
    self.assertRaises(BufferError, memio.write, b'x')
AssertionError: BufferError not raised by write

======================================================================
FAIL: test_setstate (__main__.CBytesIOTest.test_setstate)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 998, in test_setstate
    self.assertRaises(ValueError, memio.__setstate__, (b"closed", 0, None))
AssertionError: ValueError not raised by __setstate__

======================================================================
FAIL: test_write_with_export (__main__.CBytesIOTest.test_write_with_export)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 1063, in test_write_with_export
    self.assertRaises(BufferError, memio.__init__, b"replacement")
AssertionError: BufferError not raised by __init__

======================================================================
FAIL: test_setstate (__main__.CStringIOTest.test_setstate)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 1116, in test_setstate
    self.assertRaises(ValueError, memio.__setstate__, ("closed", "", 0, None))
AssertionError: ValueError not raised by __setstate__

======================================================================
FAIL: test_getbuffer (__main__.PyBytesIOTest.test_getbuffer)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 459, in test_getbuffer
    self.assertRaises(BufferError, memio.write, b'x' * 100)
AssertionError: BufferError not raised by write

======================================================================
FAIL: test_getbuffer_empty (__main__.PyBytesIOTest.test_getbuffer_empty)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_memoryio.py", line 507, in test_getbuffer_empty
    self.assertRaises(BufferError, memio.write, b'x')
AssertionError: BufferError not raised by write

----------------------------------------------------------------------
Ran 192 tests in Ns

FAILED (failures=8, errors=26, skipped=7)
