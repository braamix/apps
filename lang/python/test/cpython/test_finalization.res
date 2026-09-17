FFF...F.sss.F.ss.F
======================================================================
FAIL: test_heterogenous_resurrect_one (__main__.CycleChainFinalizationTest.test_heterogenous_resurrect_one)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_finalization.py", line 429, in test_heterogenous_resurrect_one
    self.check_resurrecting_chain([ChainedResurrector, SimpleChained] * 2)
AssertionError: unexpectedly None

======================================================================
FAIL: test_heterogenous_resurrect_three (__main__.CycleChainFinalizationTest.test_heterogenous_resurrect_three)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_finalization.py", line 436, in test_heterogenous_resurrect_three
    self.check_resurrecting_chain(
AssertionError: unexpectedly None

======================================================================
FAIL: test_heterogenous_resurrect_two (__main__.CycleChainFinalizationTest.test_heterogenous_resurrect_two)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_finalization.py", line 432, in test_heterogenous_resurrect_two
    self.check_resurrecting_chain(
AssertionError: unexpectedly None

======================================================================
FAIL: test_homogenous_resurrect (__main__.CycleChainFinalizationTest.test_homogenous_resurrect)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_finalization.py", line 416, in test_homogenous_resurrect
    self.check_resurrecting_chain([ChainedResurrector] * 3)
AssertionError: unexpectedly None

======================================================================
FAIL: test_simple_resurrect (__main__.SelfCycleFinalizationTest.test_simple_resurrect)
----------------------------------------------------------------------
AssertionError: unexpectedly None

======================================================================
FAIL: test_simple_resurrect (__main__.SimpleFinalizationTest.test_simple_resurrect)
----------------------------------------------------------------------
AssertionError: unexpectedly None

----------------------------------------------------------------------
Ran 18 tests in Ns

FAILED (failures=6, skipped=5)
