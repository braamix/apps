.......EEE...F.E.EFE.FEE...F.....E.EE.EEF....E
======================================================================
ERROR: test_copying (__main__.TestWeakSet.test_copying)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 471, in test_copying
    dup = copy.deepcopy(s)
  File "/pkg/store/python-0/lib/copy.py", line 157, in deepcopy
    y = _reconstruct(x, memo, *rv)
  File "/pkg/store/python-0/lib/copy.py", line 255, in _reconstruct
    state = deepcopy(state, memo)
  File "/pkg/store/python-0/lib/copy.py", line 131, in deepcopy
    y = copier(x, memo)
  File "/pkg/store/python-0/lib/copy.py", line 202, in _deepcopy_dict
    y[deepcopy(key, memo)] = deepcopy(value, memo)
  File "/pkg/store/python-0/lib/copy.py", line 146, in deepcopy
    rv = reductor(4)
TypeError: cannot pickle 'set' object

======================================================================
ERROR: test_difference (__main__.TestWeakSet.test_difference)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 119, in test_difference
    i = self.s.difference(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 80, in difference
    newset.difference_update(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 85, in difference_update
    self.__isub__(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 90, in __isub__
    self.data.difference_update(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_difference_update (__main__.TestWeakSet.test_difference_update)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 289, in test_difference_update
    retval = self.s.difference_update(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 85, in difference_update
    self.__isub__(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 90, in __isub__
    self.data.difference_update(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_iand (__main__.TestWeakSet.test_iand)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 281, in test_iand
    self.s &= set(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 100, in __iand__
    self.data.intersection_update(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_inplace_on_self (__main__.TestWeakSet.test_inplace_on_self)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 329, in test_inplace_on_self
    t &= t
  File "/pkg/store/python-0/lib/_weakrefset.py", line 100, in __iand__
    self.data.intersection_update(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_intersection_update (__main__.TestWeakSet.test_intersection_update)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 271, in test_intersection_update
    retval = self.s.intersection_update(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 98, in intersection_update
    self.__iand__(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 100, in __iand__
    self.data.intersection_update(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_isub (__main__.TestWeakSet.test_isub)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 300, in test_isub
    self.s -= set(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 90, in __isub__
    self.data.difference_update(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_ixor (__main__.TestWeakSet.test_ixor)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 318, in test_ixor
    self.s ^= set(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 134, in __ixor__
    self.data.symmetric_difference_update(ref(item, self._remove) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_remove (__main__.TestWeakSet.test_remove)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 235, in test_remove
    self.s.remove(x)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 65, in remove
    self.data.remove(ref(item))
KeyError: <weakref at 0xX; to 'UserString' at 0xX>

======================================================================
ERROR: test_sub (__main__.TestWeakSet.test_sub)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 127, in test_sub
    i = self.s.difference(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 80, in difference
    newset.difference_update(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 85, in difference_update
    self.__isub__(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 90, in __isub__
    self.data.difference_update(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_sub_and_super (__main__.TestWeakSet.test_sub_and_super)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 149, in test_sub_and_super
    self.assertTrue(self.ab_weakset <= self.abcde_weakset)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 104, in issubset
    return self.data.issubset(ref(item) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_symmetric_difference (__main__.TestWeakSet.test_symmetric_difference)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 132, in test_symmetric_difference
    i = self.s.symmetric_difference(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 124, in symmetric_difference
    newset.symmetric_difference_update(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 129, in symmetric_difference_update
    self.__ixor__(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 134, in __ixor__
    self.data.symmetric_difference_update(ref(item, self._remove) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_symmetric_difference_update (__main__.TestWeakSet.test_symmetric_difference_update)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 308, in test_symmetric_difference_update
    retval = self.s.symmetric_difference_update(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 129, in symmetric_difference_update
    self.__ixor__(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 134, in __ixor__
    self.data.symmetric_difference_update(ref(item, self._remove) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
ERROR: test_xor (__main__.TestWeakSet.test_xor)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 144, in test_xor
    i = self.s.symmetric_difference(self.items2)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 124, in symmetric_difference
    newset.symmetric_difference_update(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 129, in symmetric_difference_update
    self.__ixor__(other)
  File "/pkg/store/python-0/lib/_weakrefset.py", line 134, in __ixor__
    self.data.symmetric_difference_update(ref(item, self._remove) for item in other)
TypeError: 'generator' object is not an iterator

======================================================================
FAIL: test_gt (__main__.TestWeakSet.test_gt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 166, in test_gt
    self.assertTrue(self.abcde_weakset > self.ab_weakset)
AssertionError: False is not true

======================================================================
FAIL: test_intersection (__main__.TestWeakSet.test_intersection)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 98, in test_intersection
    self.assertEqual(c in i, c in self.items2 and c in self.letters)
AssertionError: False != True

======================================================================
FAIL: test_isdisjoint (__main__.TestWeakSet.test_isdisjoint)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 111, in test_isdisjoint
    self.assertTrue(not self.s.isdisjoint(WeakSet(self.letters)))
AssertionError: False is not true

======================================================================
FAIL: test_lt (__main__.TestWeakSet.test_lt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 160, in test_lt
    self.assertTrue(self.ab_weakset < self.abcde_weakset)
AssertionError: False is not true

======================================================================
FAIL: test_union (__main__.TestWeakSet.test_union)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_weakset.py", line 75, in test_union
    self.assertEqual(c in u, c in self.d or c in self.items2)
AssertionError: False != True

----------------------------------------------------------------------
Ran 46 tests in Ns

FAILED (failures=5, errors=14)
