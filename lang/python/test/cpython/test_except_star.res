.FF....F.FFFFFFFFFFFF.......................................
======================================================================
FAIL: test_break_in_except_star (__main__.TestBreakContinueReturnInExceptStarBlock.test_break_in_except_star)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 60, in test_break_in_except_star
    self.check_invalid(
  File "/tmp/test_except_star.py", line 56, in check_invalid
    with self.assertRaisesRegex(SyntaxError, self.MSG):
AssertionError: "'break', 'continue' and 'return' cannot appear in an except\* block" does not match "'break' outside loop (<string>, line 5)"

======================================================================
FAIL: test_continue_in_except_star_block_invalid (__main__.TestBreakContinueReturnInExceptStarBlock.test_continue_in_except_star_block_invalid)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 112, in test_continue_in_except_star_block_invalid
    self.check_invalid(
  File "/tmp/test_except_star.py", line 56, in check_invalid
    with self.assertRaisesRegex(SyntaxError, self.MSG):
AssertionError: "'break', 'continue' and 'return' cannot appear in an except\* block" does not match "'return' outside function (<string>, line 10)"

======================================================================
FAIL: test_exception_group_subclass_with_bad_split_func (__main__.TestExceptStarExceptionGroupSubclass.test_exception_group_subclass_with_bad_split_func)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 975, in test_exception_group_subclass_with_bad_split_func
    with self.assertRaisesRegex(TypeError, msg) as m:
AssertionError: "split must return a tuple, not str" does not match "BadEG1.split must return a 2-tuple, got str"

======================================================================
FAIL: test_raise_handle_all_raise_one_named (__main__.TestExceptStarRaise.test_raise_handle_all_raise_one_named)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 633, in test_raise_handle_all_raise_one_named
    self.assertMetadataEqual(orig, exc.__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_handle_all_raise_one_unnamed (__main__.TestExceptStarRaise.test_raise_handle_all_raise_one_unnamed)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 652, in test_raise_handle_all_raise_one_unnamed
    self.assertMetadataEqual(orig, exc.__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_handle_all_raise_two_named (__main__.TestExceptStarRaise.test_raise_handle_all_raise_two_named)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 678, in test_raise_handle_all_raise_two_named
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_handle_all_raise_two_unnamed (__main__.TestExceptStarRaise.test_raise_handle_all_raise_two_unnamed)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 705, in test_raise_handle_all_raise_two_unnamed
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_named (__main__.TestExceptStarRaise.test_raise_named)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 592, in test_raise_named
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_unnamed (__main__.TestExceptStarRaise.test_raise_unnamed)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 614, in test_raise_unnamed
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_handle_all_raise_one_named (__main__.TestExceptStarRaiseFrom.test_raise_handle_all_raise_one_named)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 790, in test_raise_handle_all_raise_one_named
    self.assertMetadataEqual(orig, exc.__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_handle_all_raise_one_unnamed (__main__.TestExceptStarRaiseFrom.test_raise_handle_all_raise_one_unnamed)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 815, in test_raise_handle_all_raise_one_unnamed
    self.assertMetadataEqual(orig, exc.__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_handle_all_raise_two_named (__main__.TestExceptStarRaiseFrom.test_raise_handle_all_raise_two_named)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 850, in test_raise_handle_all_raise_two_named
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_handle_all_raise_two_unnamed (__main__.TestExceptStarRaiseFrom.test_raise_handle_all_raise_two_unnamed)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 887, in test_raise_handle_all_raise_two_unnamed
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_named (__main__.TestExceptStarRaiseFrom.test_raise_named)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 734, in test_raise_named
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

======================================================================
FAIL: test_raise_unnamed (__main__.TestExceptStarRaiseFrom.test_raise_unnamed)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_except_star.py", line 764, in test_raise_unnamed
    self.assertMetadataEqual(orig, exc.exceptions[0].__context__)
  File "/tmp/test_except_star.py", line 182, in assertMetadataEqual
    self.assertEqual(e1.__traceback__, e2.__traceback__)
AssertionError: <traceback object at 0xX> != None

----------------------------------------------------------------------
Ran 60 tests in Ns

FAILED (failures=15)
