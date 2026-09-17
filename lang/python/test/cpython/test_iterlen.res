FFFFFFFFFF.FFFFFFFFFFF
======================================================================
FAIL: test_immutable_during_iteration (__main__.TestDeque.test_immutable_during_iteration)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 70, in test_immutable_during_iteration
    self.assertEqual(length_hint(it), n)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestDeque.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_immutable_during_iteration (__main__.TestDequeReversed.test_immutable_during_iteration)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 70, in test_immutable_during_iteration
    self.assertEqual(length_hint(it), n)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestDequeReversed.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_immutable_during_iteration (__main__.TestDictItems.test_immutable_during_iteration)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 70, in test_immutable_during_iteration
    self.assertEqual(length_hint(it), n)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestDictItems.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_immutable_during_iteration (__main__.TestDictKeys.test_immutable_during_iteration)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 70, in test_immutable_during_iteration
    self.assertEqual(length_hint(it), n)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestDictKeys.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_immutable_during_iteration (__main__.TestDictValues.test_immutable_during_iteration)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 70, in test_immutable_during_iteration
    self.assertEqual(length_hint(it), n)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestDictValues.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_issue1242657 (__main__.TestLengthHintExceptions.test_issue1242657)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 214, in test_issue1242657
    self.assertRaises(RuntimeError, list, BadLen())
AssertionError: RuntimeError not raised by list

======================================================================
FAIL: test_invariant (__main__.TestList.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_mutation (__main__.TestList.test_mutation)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 155, in test_mutation
    self.assertEqual(length_hint(it), n - 2)
AssertionError: 0 != 8

======================================================================
FAIL: test_invariant (__main__.TestListReversed.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_mutation (__main__.TestListReversed.test_mutation)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 175, in test_mutation
    self.assertEqual(length_hint(it), n - 2)
AssertionError: 0 != 8

======================================================================
FAIL: test_invariant (__main__.TestRepeat.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_immutable_during_iteration (__main__.TestSet.test_immutable_during_iteration)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 70, in test_immutable_during_iteration
    self.assertEqual(length_hint(it), n)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestSet.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestTuple.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestXrange.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

======================================================================
FAIL: test_invariant (__main__.TestXrangeCustomReversed.test_invariant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_iterlen.py", line 57, in test_invariant
    self.assertEqual(length_hint(it), i)
AssertionError: 0 != 10

----------------------------------------------------------------------
Ran 22 tests in Ns

FAILED (failures=21)
