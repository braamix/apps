.F
======================================================================
FAIL: test_single_files (__main__.TestUnicodeFiles.test_single_files)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_unicode_file.py", line 125, in test_single_files
    self._test_single(TESTFN_UNENCODABLE)
  File "/tmp/test_unicode_file.py", line 108, in _test_single
    self._do_single(filename)
  File "/tmp/test_unicode_file.py", line 54, in _do_single
    self.assertIn(base, file_list)
AssertionError: '@test_N_tmp-\udcff' not found in ['@test_N_tmp-�']

----------------------------------------------------------------------
Ran 2 tests in Ns

FAILED (failures=1)
