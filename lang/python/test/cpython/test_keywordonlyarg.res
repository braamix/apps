--- unittest ---
ok KeywordOnlyArgTestCase.testFunctionCall
error KeywordOnlyArgTestCase.testKwDefaults: AttributeError: 'function' object has no attribute '__code__'
ok KeywordOnlyArgTestCase.testRaiseErrorFuncallWithUnexpectedKeywordArgument
error KeywordOnlyArgTestCase.testSyntaxErrorForFunctionCall: NameError: name 'compile' is not defined
error KeywordOnlyArgTestCase.testSyntaxErrorForFunctionDefinition: NameError: name 'compile' is not defined
error KeywordOnlyArgTestCase.testSyntaxForManyArguments: NameError: name 'compile' is not defined
error KeywordOnlyArgTestCase.testTooManyPositionalErrorMessage: AttributeError: 'function' object has no attribute '__qualname__'
ok KeywordOnlyArgTestCase.test_default_evaluation_order
ok KeywordOnlyArgTestCase.test_issue13343
ok KeywordOnlyArgTestCase.test_kwonly_methods
ok KeywordOnlyArgTestCase.test_mangling
--- ran 11 ok 6 fail 0 error 5 skip 0 ---
