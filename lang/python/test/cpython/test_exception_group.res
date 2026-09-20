....ss.F.............F..........F....FF..FFF...FFF...
======================================================================
FAIL: test_fields_are_readonly (__main__.ExceptionGroupFields.test_fields_are_readonly)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 350, in test_fields_are_readonly
    with self.assertRaises(AttributeError):
AssertionError: AttributeError not raised

======================================================================
FAIL: test_BEG_and_E_subclass_does_not_wrap_base_exceptions (__main__.InstanceCreation.test_BEG_and_E_subclass_does_not_wrap_base_exceptions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 104, in test_BEG_and_E_subclass_does_not_wrap_base_exceptions
    with self.assertRaisesRegex(TypeError, msg):
AssertionError: TypeError not raised

======================================================================
FAIL: test_iteration_full_tracebacks (__main__.NestedExceptionGroupBasicsTest.test_iteration_full_tracebacks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 663, in test_iteration_full_tracebacks
    self.assertSequenceEqual(
AssertionError: Sequences differ: [] != [620, 607, 605]

Second sequence contains 3 additional elements.
First extra element 0:
620

- []
+ [620, 607, 605]

======================================================================
FAIL: test_split_BaseExceptionGroup (__main__.NestedExceptionGroupSplitTest.test_split_BaseExceptionGroup)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 852, in test_split_BaseExceptionGroup
    match, rest = self.split_exception_group(beg, TypeError)
  File "/tmp/test_exception_group.py", line 714, in split_exception_group
    self.assertIs(eg.__traceback__, part.__traceback__)
AssertionError: <traceback object at 0xX> is not None

======================================================================
FAIL: test_split_by_type (__main__.NestedExceptionGroupSplitTest.test_split_by_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 806, in test_split_by_type
    match, rest = self.split_exception_group(eg, SyntaxError)
  File "/tmp/test_exception_group.py", line 714, in split_exception_group
    self.assertIs(eg.__traceback__, part.__traceback__)
AssertionError: <traceback object at 0xX> is not None

======================================================================
FAIL: test_split_BaseExceptionGroup_subclass_no_derive_new_override (__main__.NestedExceptionGroupSubclassSplitTest.test_split_BaseExceptionGroup_subclass_no_derive_new_override)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 982, in test_split_BaseExceptionGroup_subclass_no_derive_new_override
    match, rest = self.split_exception_group(eg, OSError)
  File "/tmp/test_exception_group.py", line 714, in split_exception_group
    self.assertIs(eg.__traceback__, part.__traceback__)
AssertionError: <traceback object at 0xX> is not None

======================================================================
FAIL: test_split_ExceptionGroup_subclass_derive_and_new_overrides (__main__.NestedExceptionGroupSubclassSplitTest.test_split_ExceptionGroup_subclass_derive_and_new_overrides)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 1033, in test_split_ExceptionGroup_subclass_derive_and_new_overrides
    match, rest = self.split_exception_group(eg, OSError)
  File "/tmp/test_exception_group.py", line 714, in split_exception_group
    self.assertIs(eg.__traceback__, part.__traceback__)
AssertionError: <traceback object at 0xX> is not None

======================================================================
FAIL: test_split_ExceptionGroup_subclass_no_derive_no_new_override (__main__.NestedExceptionGroupSubclassSplitTest.test_split_ExceptionGroup_subclass_no_derive_no_new_override)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 943, in test_split_ExceptionGroup_subclass_no_derive_no_new_override
    match, rest = self.split_exception_group(eg, OSError)
  File "/tmp/test_exception_group.py", line 714, in split_exception_group
    self.assertIs(eg.__traceback__, part.__traceback__)
AssertionError: <traceback object at 0xX> is not None

======================================================================
FAIL: test_exceptions_mutation (__main__.StrAndReprTests.test_exceptions_mutation)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 227, in test_exceptions_mutation
    self.assertEqual(
AssertionError: "ExceptionGroup('test', (ValueError(1), TypeError(2)))" != "ExceptionGroup('test', deque([ValueError(1), TypeError(2)]))"
- ExceptionGroup('test', (ValueError(1), TypeError(2)))
+ ExceptionGroup('test', deque([ValueError(1), TypeError(2)]))
?                        +++++ +                           +


======================================================================
FAIL: test_repr_raises (__main__.StrAndReprTests.test_repr_raises)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 270, in test_repr_raises
    with self.assertRaisesRegex(
AssertionError: ".*MySeq\.__repr__\(\) must return a str, not NoneType" does not match "second argument (exceptions) must be a sequence"

======================================================================
FAIL: test_repr_small_size_args (__main__.StrAndReprTests.test_repr_small_size_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_exception_group.py", line 243, in test_repr_small_size_args
    self.assertEqual(repr(eg), "ExceptionGroup('msg', (ValueError(),))")
AssertionError: "ExceptionGroup('msg', [ValueError()])" != "ExceptionGroup('msg', (ValueError(),))"
- ExceptionGroup('msg', [ValueError()])
?                       ^            ^
+ ExceptionGroup('msg', (ValueError(),))
?                       ^            ^^


----------------------------------------------------------------------
Ran 53 tests in Ns

FAILED (failures=11, skipped=2)
