EEFF
======================================================================
ERROR: test_barry_as_bdfl (__main__.FLUFLTests.test_barry_as_bdfl)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_flufl.py", line 9, in test_barry_as_bdfl
    compile(code.format('<>'), '<BDFL test>', 'exec',
  File "<BDFL test>", line 2
    2 <> 3
       ^^^
SyntaxError: invalid syntax

======================================================================
ERROR: test_barry_as_bdfl_look_ma_with_no_compiler_flags (__main__.FLUFLTests.test_barry_as_bdfl_look_ma_with_no_compiler_flags)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_flufl.py", line 41, in test_barry_as_bdfl_look_ma_with_no_compiler_flags
    compile(code.format('<>'), '<BDFL test>', 'exec')
  File "<BDFL test>", line 1
    from __future__ import barry_as_FLUFL;2 <> 3
                                             ^^^
SyntaxError: invalid syntax

======================================================================
FAIL: test_barry_as_bdfl_relative_import (__main__.FLUFLTests.test_barry_as_bdfl_relative_import)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_flufl.py", line 59, in test_barry_as_bdfl_relative_import
    self.assertEqual(cm.exception.offset, len(code) - 4)
AssertionError: 43 != 42

======================================================================
FAIL: test_guido_as_bdfl (__main__.FLUFLTests.test_guido_as_bdfl)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_flufl.py", line 35, in test_guido_as_bdfl
    self.assertEqual(cm.exception.offset, 3)
AssertionError: 4 != 3

----------------------------------------------------------------------
Ran 4 tests in Ns

FAILED (failures=2, errors=2)
