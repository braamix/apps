.............F...........F...
======================================================================
FAIL: test_unfinished_generator (__main__.ProfileHookTestCase.test_unfinished_generator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 272, in test_unfinished_generator
    self.check_events(g, [(1, 'call', g_ident, None),
  File "/tmp/test_sys_setprofile.py", line 96, in check_events
    self.fail("Expected events:\n%s\nReceived events:\n%s"
AssertionError: Expected events:
[(1, 'call', (267, 'g'), None),
 (2, 'call', (264, 'f'), None),
 (2, 'return', (264, 'f'), 0),
 (2, 'call', (264, 'f'), None),
 (2, 'return', (264, 'f'), None),
 (1, 'return', (267, 'g'), None)]
Received events:
[(1, 'call', (267, 'g'), None),
 (2, 'call', (264, 'f'), None),
 (2, 'return', (264, 'f'), 0),
 (1, 'return', (267, 'g'), None)]

======================================================================
FAIL: test_reentrancy (__main__.TestEdgeCases.test_reentrancy)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 462, in test_reentrancy
    self.assertEqual(sys.getprofile(), bar)
AssertionError: <function TestEdgeCases.test_reentrancy.<locals>.foo> != <function TestEdgeCases.test_reentrancy.<locals>.bar>

----------------------------------------------------------------------
Ran 29 tests in Ns

FAILED (failures=2)
