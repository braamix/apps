...F....FFFF..FFF
======================================================================
FAIL: test_future_imports (__main__.CodeopTests.test_future_imports) (compiler=<Compile object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 384, in test_future_imports
    self.assertGreater(compiler.flags, original_flags)
AssertionError: 16896 not greater than 16896

======================================================================
FAIL: test_invalid_warning (__main__.CodeopTests.test_invalid_warning) (compiler=<function compile_command>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 364, in test_invalid_warning
    self.assertEqual(len(w), 1)
AssertionError: 0 != 1

======================================================================
FAIL: test_invalid_warning (__main__.CodeopTests.test_invalid_warning) (compiler=<CommandCompiler object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 364, in test_invalid_warning
    self.assertEqual(len(w), 1)
AssertionError: 0 != 1

======================================================================
FAIL: test_invalid_warning (__main__.CodeopTests.test_invalid_warning) (compiler=<Compile object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 364, in test_invalid_warning
    self.assertEqual(len(w), 1)
AssertionError: 0 != 1

======================================================================
FAIL: test_raw_raises_error (__main__.CodeopTests.test_raw_raises_error) (compiler=<Compile object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 357, in test_raw_raises_error
    self.assertEqual(len(w.warnings), 1)
AssertionError: 0 != 1

======================================================================
FAIL: test_warning (__main__.CodeopTests.test_warning) (compiler=<function compile_command>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 326, in test_warning
    with warnings_helper.check_warnings(
  File "/pkg/store/python-0/lib/contextlib.py", line 210, in __exit__
    for _ in self.gen:
  File "/tmp/test/support/warnings_helper.py", line 107, in _filterwarnings
    raise AssertionError("filter (%r, %s) did not catch any warning" % missing[0])
AssertionError: filter ('"is" with \'str\' literal', SyntaxWarning) did not catch any warning

======================================================================
FAIL: test_warning (__main__.CodeopTests.test_warning) (compiler=<CommandCompiler object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 326, in test_warning
    with warnings_helper.check_warnings(
  File "/pkg/store/python-0/lib/contextlib.py", line 210, in __exit__
    for _ in self.gen:
  File "/tmp/test/support/warnings_helper.py", line 107, in _filterwarnings
    raise AssertionError("filter (%r, %s) did not catch any warning" % missing[0])
AssertionError: filter ('"is" with \'str\' literal', SyntaxWarning) did not catch any warning

======================================================================
FAIL: test_warning (__main__.CodeopTests.test_warning) (compiler=<Compile object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 252, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_codeop.py", line 326, in test_warning
    with warnings_helper.check_warnings(
  File "/pkg/store/python-0/lib/contextlib.py", line 210, in __exit__
    for _ in self.gen:
  File "/tmp/test/support/warnings_helper.py", line 107, in _filterwarnings
    raise AssertionError("filter (%r, %s) did not catch any warning" % missing[0])
AssertionError: filter ('"is" with \'str\' literal', SyntaxWarning) did not catch any warning

----------------------------------------------------------------------
Ran 13 tests in Ns

FAILED (failures=8)
