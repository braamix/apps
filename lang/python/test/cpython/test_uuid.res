sssssssssssssE...ssss...ssssssssssssssssssssssssssssssssssssssssssssssssss.......................s.sss.s.......F........
======================================================================
ERROR: test_arp_getnode (__main__.TestInternalsWithoutExtModule.test_arp_getnode)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_uuid.py", line 1507, in test_arp_getnode
    node = self.uuid._arp_getnode()
  File "/pkg/store/python-0/lib/uuid.py", line 595, in _arp_getnode
    import os, socket
ModuleNotFoundError: Standard library module 'socket' was not found

======================================================================
FAIL: test_uuid6_uniqueness (__main__.TestUUIDWithoutExtModule.test_uuid6_uniqueness)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_uuid.py", line 795, in test_uuid6_uniqueness
    self.assertLess(len(uuids), N, 'collision property does not hold')
AssertionError: 1024 not less than 1024 : collision property does not hold

----------------------------------------------------------------------
Ran 120 tests in Ns

FAILED (failures=1, errors=1, skipped=72)
