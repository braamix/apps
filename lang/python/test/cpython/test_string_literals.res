.FF.F.FF..F......F..
======================================================================
FAIL: test_eval_bytes_invalid_escape (__main__.TestLiterals.test_eval_bytes_invalid_escape)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_string_literals.py", line 230, in test_eval_bytes_invalid_escape
    with self.assertWarns(SyntaxWarning):
AssertionError: SyntaxWarning not triggered

======================================================================
FAIL: test_eval_bytes_invalid_octal_escape (__main__.TestLiterals.test_eval_bytes_invalid_octal_escape)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_string_literals.py", line 256, in test_eval_bytes_invalid_octal_escape
    with self.assertWarns(SyntaxWarning):
AssertionError: SyntaxWarning not triggered

======================================================================
FAIL: test_eval_bytes_raw (__main__.TestLiterals.test_eval_bytes_raw)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_string_literals.py", line 299, in test_eval_bytes_raw
    self.assertRaises(SyntaxError, eval, """ bb'' """)
AssertionError: SyntaxError not raised by eval

======================================================================
FAIL: test_eval_str_invalid_escape (__main__.TestLiterals.test_eval_str_invalid_escape)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_string_literals.py", line 112, in test_eval_str_invalid_escape
    with self.assertWarns(SyntaxWarning):
AssertionError: SyntaxWarning not triggered

======================================================================
FAIL: test_eval_str_invalid_octal_escape (__main__.TestLiterals.test_eval_str_invalid_octal_escape)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_string_literals.py", line 151, in test_eval_str_invalid_octal_escape
    with self.assertWarns(SyntaxWarning):
AssertionError: SyntaxWarning not triggered

======================================================================
FAIL: test_eval_str_u (__main__.TestLiterals.test_eval_str_u)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_string_literals.py", line 310, in test_eval_str_u
    self.assertRaises(SyntaxError, eval, """ ur'' """)
AssertionError: SyntaxError not raised by eval

======================================================================
FAIL: test_invalid_escape_locations_with_offset (__main__.TestLiterals.test_invalid_escape_locations_with_offset)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_string_literals.py", line 181, in test_invalid_escape_locations_with_offset
    self.assertEqual(len(w), 1)
AssertionError: 0 != 1

----------------------------------------------------------------------
Ran 20 tests in Ns

FAILED (failures=7)
