ssssssssssssss...ssss...ssssssssssssssssssssssssssssssssssssssssssssssssss.......................s.sss.s.......F........
======================================================================
FAIL: test_uuid6_uniqueness (__main__.TestUUIDWithoutExtModule.test_uuid6_uniqueness)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_uuid.py", line 795, in test_uuid6_uniqueness
    self.assertLess(len(uuids), N, 'collision property does not hold')
AssertionError: 1024 not less than 1024 : collision property does not hold

----------------------------------------------------------------------
Ran 120 tests in Ns

FAILED (failures=1, skipped=73)
