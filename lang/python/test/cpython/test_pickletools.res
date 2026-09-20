...............................................s........FFF....ssssss.....ssssss...s.................ssssss....ssssss.............ss..........sssss.....................................................................
======================================================================
FAIL: test_compat_pickle (__main__.OptimizedPickleTests.test_compat_pickle) (type=<class 'map'>, proto=0)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/pickletester.py", line 4426, in test_compat_pickle
    self.assertIn(('c%s\n%s' % (mod, name)).encode(), pickled)
AssertionError: b'citertools\nimap' not found in b'c__builtin__\niter\n((lI1\naI2\naI3\natR.'

======================================================================
FAIL: test_compat_pickle (__main__.OptimizedPickleTests.test_compat_pickle) (type=<class 'map'>, proto=1)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/pickletester.py", line 4426, in test_compat_pickle
    self.assertIn(('c%s\n%s' % (mod, name)).encode(), pickled)
AssertionError: b'citertools\nimap' not found in b'c__builtin__\niter\n(](K\x01K\x02K\x03etR.'

======================================================================
FAIL: test_compat_pickle (__main__.OptimizedPickleTests.test_compat_pickle) (type=<class 'map'>, proto=2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/pickletester.py", line 4426, in test_compat_pickle
    self.assertIn(('c%s\n%s' % (mod, name)).encode(), pickled)
AssertionError: b'citertools\nimap' not found in b'\x80\x02c__builtin__\niter\n](K\x01K\x02K\x03e\x85R.'

----------------------------------------------------------------------
Ran 204 tests in Ns

FAILED (failures=3, skipped=33)
