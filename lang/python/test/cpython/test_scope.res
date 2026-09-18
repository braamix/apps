....sEF.E..F.Es...Es............E...EE.s.
======================================================================
ERROR: testClassAndGlobal (__main__.ScopeTests.testClassAndGlobal)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 482, in testClassAndGlobal
    exec("""if 1:
  File "<string>", line 10, in <module>
  File "<string>", line 6, in __call__
NameError: name 'x' is not defined

======================================================================
ERROR: testEvalExecFreeVars (__main__.ScopeTests.testEvalExecFreeVars)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 614, in testEvalExecFreeVars
    self.assertRaises(TypeError, eval, g.__code__)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
SystemError: closure does not match the code object

======================================================================
ERROR: testGlobalInParallelNestedFunctions (__main__.ScopeTests.testGlobalInParallelNestedFunctions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 702, in testGlobalInParallelNestedFunctions
    exec("""if 1:
  File "<string>", line 12, in <module>
  File "<string>", line 6, in g
NameError: name 'y' is not defined

======================================================================
ERROR: testLocalsClass (__main__.ScopeTests.testLocalsClass)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 540, in testLocalsClass
    self.assertEqual(f(1).x, 12)
AttributeError: type object 'C' has no attribute 'x'

======================================================================
ERROR: testScopeOfGlobalStmt (__main__.ScopeTests.testScopeOfGlobalStmt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 376, in testScopeOfGlobalStmt
    exec("""if 1:
  File "<string>", line 14, in <module>
  File "<string>", line 13, in f
  File "<string>", line 12, in g
  File "<string>", line 11, in i
  File "<string>", line 10, in h
NameError: name 'x' is not defined

======================================================================
ERROR: testUnboundLocal (__main__.ScopeTests.testUnboundLocal)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 319, in testUnboundLocal
    self.assertRaises(UnboundLocalError, errorInOuter)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_scope.py", line 308, in errorInOuter
    print(y)
NameError: local variable 'y' referenced before assignment

======================================================================
ERROR: testUnboundLocal_AfterDel (__main__.ScopeTests.testUnboundLocal_AfterDel)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 340, in testUnboundLocal_AfterDel
    self.assertRaises(UnboundLocalError, errorInOuter)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_scope.py", line 329, in errorInOuter
    print(y)
NameError: local variable 'y' referenced before assignment

======================================================================
FAIL: testClassNamespaceOverridesClosure (__main__.ScopeTests.testClassNamespaceOverridesClosure)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 777, in testClassNamespaceOverridesClosure
    self.assertEqual(X.y, 43)
AssertionError: 42 != 43

======================================================================
FAIL: testFreeVarInMethod (__main__.ScopeTests.testFreeVarInMethod)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_scope.py", line 148, in testFreeVarInMethod
    self.assertEqual(t.test(), "var")
AssertionError: <function ScopeTests.testFreeVarInMethod.[38 chars]_var> != 'var'

----------------------------------------------------------------------
Ran 41 tests in Ns

FAILED (failures=2, errors=7, skipped=4)
