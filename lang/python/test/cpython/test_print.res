...FFFFFF
======================================================================
FAIL: test_normal_string (__main__.TestPy2MigrationHint.test_normal_string)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_print.py", line 154, in test_normal_string
    self.assertIn("Missing parentheses in call to 'print'. Did you mean print(...)",
AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'

======================================================================
FAIL: test_string_in_loop_on_same_line (__main__.TestPy2MigrationHint.test_string_in_loop_on_same_line)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_print.py", line 199, in test_string_in_loop_on_same_line
    self.assertIn("Missing parentheses in call to 'print'. Did you mean print(...)",
AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'

======================================================================
FAIL: test_string_with_excessive_whitespace (__main__.TestPy2MigrationHint.test_string_with_excessive_whitespace)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_print.py", line 170, in test_string_with_excessive_whitespace
    self.assertIn("Missing parentheses in call to 'print'. Did you mean print(...)",
AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'

======================================================================
FAIL: test_string_with_leading_whitespace (__main__.TestPy2MigrationHint.test_string_with_leading_whitespace)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_print.py", line 180, in test_string_with_leading_whitespace
    self.assertIn("Missing parentheses in call to 'print'. Did you mean print(...)",
AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 2)'

======================================================================
FAIL: test_string_with_semicolon (__main__.TestPy2MigrationHint.test_string_with_semicolon)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_print.py", line 191, in test_string_with_semicolon
    self.assertIn("Missing parentheses in call to 'print'. Did you mean print(...)",
AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'

======================================================================
FAIL: test_string_with_soft_space (__main__.TestPy2MigrationHint.test_string_with_soft_space)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_print.py", line 162, in test_string_with_soft_space
    self.assertIn("Missing parentheses in call to 'print'. Did you mean print(...)",
AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'

----------------------------------------------------------------------
Ran 9 tests in Ns

FAILED (failures=6)
