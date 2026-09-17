--- unittest ---
ok ScopeTests.testBoundAndFree
ok ScopeTests.testCellIsArgAndEscapes
ok ScopeTests.testCellIsKwonlyArg
ok ScopeTests.testCellIsLocalAndEscapes
skip ScopeTests.testCellLeak: implementation detail of CPython
error ScopeTests.testClassAndGlobal: NameError: name 'x' is not defined
fail ScopeTests.testClassNamespaceOverridesClosure: AssertionError: 42 != 43
ok ScopeTests.testComplexDefinitions
error ScopeTests.testEvalExecFreeVars: SystemError: closure does not match the code object
ok ScopeTests.testEvalFreeVars
ok ScopeTests.testExtraNesting
fail ScopeTests.testFreeVarInMethod: AssertionError: <function ScopeTests.testFreeVarInMethod.<locals>.test.<locals>.Test.method_and_var> != 'var'
ok ScopeTests.testFreeingCell
error ScopeTests.testGlobalInParallelNestedFunctions: NameError: name 'y' is not defined
skip ScopeTests.testInteractionWithTraceFunc: implementation detail of CPython
ok ScopeTests.testLambdas
ok ScopeTests.testLeaks
ok ScopeTests.testListCompLocalVars
error ScopeTests.testLocalsClass: AttributeError: 'type' object has no attribute 'x'
skip ScopeTests.testLocalsClass_WithTrace: implementation detail of CPython
ok ScopeTests.testLocalsFunction
ok ScopeTests.testMixedFreevarsAndCellvars
ok ScopeTests.testNearestEnclosingScope
ok ScopeTests.testNestedNonLocal
ok ScopeTests.testNestingGlobalNoFree
ok ScopeTests.testNestingPlusFreeRefToGlobal
ok ScopeTests.testNestingThroughClass
ok ScopeTests.testNonLocalClass
ok ScopeTests.testNonLocalFunction
ok ScopeTests.testNonLocalGenerator
ok ScopeTests.testNonLocalMethod
ok ScopeTests.testRecursion
error ScopeTests.testScopeOfGlobalStmt: NameError: name 'x' is not defined
ok ScopeTests.testSimpleAndRebinding
ok ScopeTests.testSimpleNesting
ok ScopeTests.testTopIsNotSignificant
error ScopeTests.testUnboundLocal: NameError: local variable 'y' referenced before assignment
error ScopeTests.testUnboundLocal_AfterDel: NameError: local variable 'y' referenced before assignment
ok ScopeTests.testUnboundLocal_AugAssign
skip ScopeTests.testUnoptimizedNamespaces: check_syntax_error needs compile()
ok ScopeTests.test_multiple_nesting
--- ran 41 ok 28 fail 2 error 7 skip 4 ---
