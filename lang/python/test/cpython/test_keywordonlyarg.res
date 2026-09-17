...FF.F....
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

======================================================================
FAIL: testTooManyPositionalErrorMessage (__main__.KeywordOnlyArgTestCase.testTooManyPositionalErrorMessage)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_keywordonlyarg.py", line 68, in testTooManyPositionalErrorMessage
    self.assertEqual(str(exc.exception), expected)
AssertionError: 'f() takes 2 positional arguments but 3 were given' != 'KeywordOnlyArgTestCase.testTooManyPositio[80 chars]iven'
- f() takes 2 positional arguments but 3 were given
+ KeywordOnlyArgTestCase.testTooManyPositionalErrorMessage.<locals>.f() takes from 1 to 2 positional arguments but 3 were given


----------------------------------------------------------------------
Ran 11 tests in Ns

FAILED (failures=3)
