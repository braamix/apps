...FF......
======================================================================
FAIL: testSyntaxErrorForFunctionCall (__main__.KeywordOnlyArgTestCase.testSyntaxErrorForFunctionCall)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_keywordonlyarg.py", line 72, in testSyntaxErrorForFunctionCall
    self.assertRaisesSyntaxError("f(p, k1=50, *(1,2), k1=100)")
  File "/tmp/test_keywordonlyarg.py", line 41, in assertRaisesSyntaxError
    self.assertRaises(SyntaxError, shouldRaiseSyntaxError, codestr)
AssertionError: SyntaxError not raised by shouldRaiseSyntaxError

======================================================================
FAIL: testSyntaxErrorForFunctionDefinition (__main__.KeywordOnlyArgTestCase.testSyntaxErrorForFunctionDefinition)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_keywordonlyarg.py", line 44, in testSyntaxErrorForFunctionDefinition
    self.assertRaisesSyntaxError("def f(p, *):\n  pass\n")
  File "/tmp/test_keywordonlyarg.py", line 41, in assertRaisesSyntaxError
    self.assertRaises(SyntaxError, shouldRaiseSyntaxError, codestr)
AssertionError: SyntaxError not raised by shouldRaiseSyntaxError

----------------------------------------------------------------------
Ran 11 tests in Ns

FAILED (failures=2)
