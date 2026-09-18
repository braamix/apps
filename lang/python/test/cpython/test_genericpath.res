..........FF........sFF.sF
======================================================================
FAIL: test_exists_bool (__main__.TestGenericTest.test_exists_bool)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_genericpath.py", line 181, in test_exists_bool
    with self.assertWarnsRegex(RuntimeWarning,
AssertionError: RuntimeWarning not triggered

======================================================================
FAIL: test_exists_fd (__main__.TestGenericTest.test_exists_fd)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_genericpath.py", line 173, in test_exists_fd
    self.assertTrue(self.pathmodule.exists(r))
AssertionError: False is not true

======================================================================
FAIL: test_samefile_on_symlink (__main__.TestGenericTest.test_samefile_on_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_genericpath.py", line 266, in test_samefile_on_symlink
    self._test_samefile_on_link_func(os.symlink)
  File "/tmp/test_genericpath.py", line 258, in _test_samefile_on_link_func
    self.assertTrue(self.pathmodule.samefile(test_fn1, test_fn2))
AssertionError: False is not true

======================================================================
FAIL: test_sameopenfile (__main__.TestGenericTest.test_sameopenfile)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_genericpath.py", line 327, in test_sameopenfile
    self.assertTrue(self.pathmodule.sameopenfile(fd1, fd2))
AssertionError: False is not true

======================================================================
FAIL: test_samestat_on_symlink (__main__.TestGenericTest.test_samestat_on_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_genericpath.py", line 309, in test_samestat_on_symlink
    self._test_samestat_on_link_func(os.symlink)
  File "/tmp/test_genericpath.py", line 299, in _test_samestat_on_link_func
    self.assertTrue(self.pathmodule.samestat(os.stat(test_fn1),
AssertionError: False is not true

----------------------------------------------------------------------
Ran 26 tests in Ns

FAILED (failures=5, skipped=2)
