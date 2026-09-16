--- unittest ---
ok KeywordOnlyArgTestCase.testFunctionCall
ok KeywordOnlyArgTestCase.testKwDefaults
ok KeywordOnlyArgTestCase.testRaiseErrorFuncallWithUnexpectedKeywordArgument
fail KeywordOnlyArgTestCase.testSyntaxErrorForFunctionCall: AssertionError: SyntaxError not raised
fail KeywordOnlyArgTestCase.testSyntaxErrorForFunctionDefinition: AssertionError: SyntaxError not raised
ok KeywordOnlyArgTestCase.testSyntaxForManyArguments
fail KeywordOnlyArgTestCase.testTooManyPositionalErrorMessage: AssertionError: 'f() takes 2 positional arguments but 3 were given' != 'KeywordOnlyArgTestCase.testTooManyPositionalErrorMessage.<locals>.f() takes from 1 to 2 positional arguments but 3 were given'
ok KeywordOnlyArgTestCase.test_default_evaluation_order
ok KeywordOnlyArgTestCase.test_issue13343
ok KeywordOnlyArgTestCase.test_kwonly_methods
ok KeywordOnlyArgTestCase.test_mangling
--- ran 11 ok 8 fail 3 error 0 skip 0 ---
