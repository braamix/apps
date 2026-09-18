...F.FFFF.............E.......E.EEEsEsEEEE...........F.E.........sssssssE.s..
======================================================================
ERROR: test_reference_loop_code (__main__.BugsTestCase.test_reference_loop_code)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 379, in test_reference_loop_code
    code = code.replace(co_consts=code.co_consts + (a,))
TypeError: replace() got an unexpected keyword argument 'co_consts'

======================================================================
ERROR: test_unmarshallable (__main__.BugsTestCase.test_unmarshallable)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 570, in test_unmarshallable
    code = code.replace(co_consts=(1, fset, None))
TypeError: replace() got an unexpected keyword argument 'co_consts'

======================================================================
ERROR: test_code (__main__.CodeTestCase.test_code)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 135, in test_code
    new = marshal.loads(marshal.dumps(co))
ValueError: unmarshallable object

======================================================================
ERROR: test_different_filenames (__main__.CodeTestCase.test_different_filenames)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 147, in test_different_filenames
    co1, co2 = marshal.loads(marshal.dumps((co1, co2)))
ValueError: unmarshallable object

======================================================================
ERROR: test_many_codeobjects (__main__.CodeTestCase.test_many_codeobjects)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 142, in test_many_codeobjects
    marshal.loads(marshal.dumps(codes))
ValueError: unmarshallable object

======================================================================
ERROR: test_no_allow_code (__main__.CodeTestCase.test_no_allow_code)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 163, in test_no_allow_code
    dump = marshal.dumps(data, allow_code=True)
ValueError: unmarshallable object

======================================================================
ERROR: test0To3 (__main__.CompatibilityTestCase.test0To3)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 758, in test0To3
    self._test(0)
  File "/tmp/test_marshal.py", line 754, in _test
    data = marshal.dumps(code, version)
ValueError: unmarshallable object

======================================================================
ERROR: test1To3 (__main__.CompatibilityTestCase.test1To3)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 761, in test1To3
    self._test(1)
  File "/tmp/test_marshal.py", line 754, in _test
    data = marshal.dumps(code, version)
ValueError: unmarshallable object

======================================================================
ERROR: test2To3 (__main__.CompatibilityTestCase.test2To3)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 764, in test2To3
    self._test(2)
  File "/tmp/test_marshal.py", line 754, in _test
    data = marshal.dumps(code, version)
ValueError: unmarshallable object

======================================================================
ERROR: test3To3 (__main__.CompatibilityTestCase.test3To3)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 767, in test3To3
    self._test(3)
  File "/tmp/test_marshal.py", line 754, in _test
    data = marshal.dumps(code, version)
ValueError: unmarshallable object

======================================================================
ERROR: testModule (__main__.InstancingTestCase.testModule)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 736, in testModule
    self.helper(code)
  File "/tmp/test_marshal.py", line 20, in helper
    new = marshal.loads(marshal.dumps(sample, *extra))
ValueError: unmarshallable object

======================================================================
ERROR: test_slice (__main__.SliceTestCase.test_slice) (obj="slice({'set'}, ('tuple', {'with': 'dict'}), <code helper at line 19>)")
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 794, in test_slice
    self.helper(obj)
  File "/tmp/test_marshal.py", line 20, in helper
    new = marshal.loads(marshal.dumps(sample, *extra))
ValueError: unmarshallable object

======================================================================
FAIL: test_bad_reader (__main__.BugsTestCase.test_bad_reader)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 529, in test_bad_reader
    self.assertRaises(ValueError, marshal.load,
AssertionError: ValueError not raised by load

======================================================================
FAIL: test_deterministic_sets (__main__.BugsTestCase.test_deterministic_sets) [set([float('nan'), b'a', b'b', b'c', 'x', 'y', 'z'])]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 558, in test_deterministic_sets
    self.assertNotEqual(repr_0, repr_1)
AssertionError: b"{nan, b'a', b'b', b'c', 'x', 'y', 'z'}\n" == b"{nan, b'a', b'b', b'c', 'x', 'y', 'z'}\n"

======================================================================
FAIL: test_deterministic_sets (__main__.BugsTestCase.test_deterministic_sets) [set([('Spam', 0), ('Spam', 1), ('Spam', 2), ('Spam', 3), ('Spam', 4), ('Spam', 5)])]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 558, in test_deterministic_sets
    self.assertNotEqual(repr_0, repr_1)
AssertionError: b"{('Spam', 0), ('Spam', 1), ('Spam', 2), ('Spam', 3), ('Spam', 4), ('Spam', 5)}\n" == b"{('Spam', 0), ('Spam', 1), ('Spam', 2), ('Spam', 3), ('Spam', 4), ('Spam', 5)}\n"

======================================================================
FAIL: test_deterministic_sets (__main__.BugsTestCase.test_deterministic_sets) [frozenset([float('nan'), b'a', b'b', b'c', 'x', 'y', 'z'])]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 558, in test_deterministic_sets
    self.assertNotEqual(repr_0, repr_1)
AssertionError: b"frozenset({nan, b'a', b'b', b'c', 'x', 'y', 'z'})\n" == b"frozenset({nan, b'a', b'b', b'c', 'x', 'y', 'z'})\n"

======================================================================
FAIL: test_deterministic_sets (__main__.BugsTestCase.test_deterministic_sets) [frozenset([('Spam', 0), ('Spam', 1), ('Spam', 2), ('Spam', 3), ('Spam', 4), ('Spam', 5)])]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 558, in test_deterministic_sets
    self.assertNotEqual(repr_0, repr_1)
AssertionError: b"frozenset({('Spam', 0), ('Spam', 1), ('Spam', 2), ('Spam', 3), ('Spam', 4), ('Spam', 5)})\n" == b"frozenset({('Spam', 0), ('Spam', 1), ('Spam', 2), ('Spam', 3), ('Spam', 4), ('Spam', 5)})\n"

======================================================================
FAIL: testInt (__main__.InstancingTestCase.testInt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_marshal.py", line 675, in testInt
    self.helper3(intobj, simple=True)
  File "/tmp/test_marshal.py", line 664, in helper3
    self.assertGreater(n2, n0)
AssertionError: 2 not greater than 2

----------------------------------------------------------------------
Ran 74 tests in Ns

FAILED (failures=6, errors=12, skipped=10)
