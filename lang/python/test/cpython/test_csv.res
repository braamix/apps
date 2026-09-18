...ss.E..........................................E...............................ssss...............................F....................E...........E......
======================================================================
ERROR: test_char_write (__main__.TestArrayWrites.test_char_write)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_csv.py", line 1193, in test_char_write
    a = array.array('w', string.ascii_letters)
ValueError: bad typecode (must be b, B, u, h, H, i, I, l, L, q, Q, f or d)

======================================================================
ERROR: test_dialect_getattr_non_attribute_error_propagates (__main__.TestDialectValidity.test_dialect_getattr_non_attribute_error_propagates)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_csv.py", line 1392, in test_dialect_getattr_non_attribute_error_propagates
    csv.reader([], dialect=BadDialect())
TypeError: a dialect attribute that needs a call: delimiter

======================================================================
ERROR: test_reader_reentrant_iterator (__main__.Test_Csv.test_reader_reentrant_iterator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_csv.py", line 596, in test_reader_reentrant_iterator
    reader = csv.reader(it)
  File "/tmp/test_csv.py", line 587, in __next__
    next(self.reader)
TypeError: 'NoneType' object is not an iterator

======================================================================
ERROR: test_writer_arg_valid (__main__.Test_Csv.test_writer_arg_valid)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_csv.py", line 100, in test_writer_arg_valid
    self.assertRaises(OSError, csv.writer, BadWriter())
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: argument 1 must have a "write" method

======================================================================
FAIL: test_sniff_truncated_sample (__main__.TestSniffer.test_sniff_truncated_sample)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_csv.py", line 1781, in test_sniff_truncated_sample
    self.assertEqual(dialect.escapechar, '\\')
AssertionError: None != '\\'

----------------------------------------------------------------------
Ran 156 tests in Ns

FAILED (failures=1, errors=4, skipped=6)
