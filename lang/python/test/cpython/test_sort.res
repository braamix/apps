Testing size 0
    checking identity
    checking reversed
    checking random permutation
    checking reversed via function
    Checking against an insane comparison function.
        If the implementation isn't careful, this may segfault.
    checking an insane function left some permutation
    checking stability
Testing size 1
    checking identity
    checking reversed
    checking random permutation
    checking reversed via function
    Checking against an insane comparison function.
        If the implementation isn't careful, this may segfault.
    checking an insane function left some permutation
    checking stability
Testing size 2
    checking identity
    checking reversed
    checking random permutation
    checking reversed via function
    Checking against an insane comparison function.
        If the implementation isn't careful, this may segfault.
    checking an insane function left some permutation
E.EF...F.F.........F.
======================================================================
ERROR: testStressfully (__main__.TestBase.testStressfully)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sort.py", line 127, in testStressfully
    augmented.sort()    # forced stable because ties broken by index
TypeError: '<' not supported between instances of 'Stable' and 'Stable'

======================================================================
ERROR: test_bug453523 (__main__.TestBugs.test_bug453523)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sort.py", line 170, in test_bug453523
    self.assertRaises(ValueError, L.sort)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: '<' not supported between instances of 'int' and 'C'

======================================================================
FAIL: test_undetected_mutation (__main__.TestBugs.test_undetected_mutation)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sort.py", line 181, in test_undetected_mutation
    self.assertRaises(ValueError, L.sort, key=cmp_to_key(mutating_cmp))
AssertionError: ValueError not raised by sort

======================================================================
FAIL: test_key_with_mutating_del (__main__.TestDecorateSortUndecorate.test_key_with_mutating_del)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sort.py", line 239, in test_key_with_mutating_del
    self.assertRaises(ValueError, data.sort, key=SortKiller)
AssertionError: ValueError not raised by sort

======================================================================
FAIL: test_key_with_mutation (__main__.TestDecorateSortUndecorate.test_key_with_mutation)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sort.py", line 227, in test_key_with_mutation
    self.assertRaises(ValueError, data.sort, key=k)
AssertionError: ValueError not raised by sort

======================================================================
FAIL: test_unsafe_object_compare (__main__.TestOptimizedCompares.test_unsafe_object_compare)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sort.py", line 348, in test_unsafe_object_compare
    with self.assertRaises(ValueError):
AssertionError: ValueError not raised

----------------------------------------------------------------------
Ran 21 tests in Ns

FAILED (failures=4, errors=2)
