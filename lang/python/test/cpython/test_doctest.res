..........FF................................EFF.FFF..F....FFF..ssss..F.FFFFFF.....EEEException ignored in a finalizer:
OSError: [Errno 9] Bad file descriptor
EEException ignored in a finalizer:
OSError: [Errno 9] Bad file descriptor
EEException ignored in a finalizer:
OSError: [Errno 9] Bad file descriptor
E...F.F.Exception ignored in a finalizer:
OSError: [Errno 9] Bad file descriptor
.....F..F.EEE......
======================================================================
ERROR: test_DocFileSuite (__main__) [6]
Doctest: __main__.test_DocFileSuite
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2713, in __main__.test_DocFileSuite
    >>> suite = doctest.DocFileSuite('../test_doctest/test_doctest.txt')
  File "<doctest __main__.test_DocFileSuite[6]>", line 1, in <module>
    suite = doctest.DocFileSuite('../test_doctest/test_doctest.txt')
  File "/pkg/store/python-0/lib/doctest.py", line 2733, in DocFileSuite
    suite.addTest(DocFileTest(path, **kw))
  File "/pkg/store/python-0/lib/doctest.py", line 2655, in DocFileTest
    doc, path = _load_testfile(path, package, module_relative,
  File "/pkg/store/python-0/lib/doctest.py", line 254, in _load_testfile
    with open(filename, encoding=encoding) as f:
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/../test_doctest/test_doctest.txt'

======================================================================
ERROR: test_lineendings (__main__) [2]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3330, in __main__.test_lineendings
    >>> with open(fn, 'wb') as f:
  File "<doctest __main__.test_lineendings[2]>", line 1, in <module>
    with open(fn, 'wb') as f:
TypeError: 'NoneType' object is not callable

======================================================================
ERROR: test_lineendings (__main__) [3]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3333, in __main__.test_lineendings
    >>> doctest.testfile(fn, module_relative=False, verbose=False)
  File "<doctest __main__.test_lineendings[3]>", line 1, in <module>
    doctest.testfile(fn, module_relative=False, verbose=False)
  File "/pkg/store/python-0/lib/doctest.py", line 2211, in testfile
    text, filename = _load_testfile(filename, package, module_relative,
  File "/pkg/store/python-0/lib/doctest.py", line 254, in _load_testfile
    with open(filename, encoding=encoding) as f:
PermissionError: [Errno 13] Permission denied: '/tmp/tmp2rg9lw53'

======================================================================
ERROR: test_lineendings (__main__) [6]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3340, in __main__.test_lineendings
    >>> with open(fn, 'wb') as f:
  File "<doctest __main__.test_lineendings[6]>", line 1, in <module>
    with open(fn, 'wb') as f:
TypeError: 'NoneType' object is not callable

======================================================================
ERROR: test_lineendings (__main__) [7]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3343, in __main__.test_lineendings
    >>> doctest.testfile(fn, module_relative=False, verbose=False)
  File "<doctest __main__.test_lineendings[7]>", line 1, in <module>
    doctest.testfile(fn, module_relative=False, verbose=False)
  File "/pkg/store/python-0/lib/doctest.py", line 2211, in testfile
    text, filename = _load_testfile(filename, package, module_relative,
  File "/pkg/store/python-0/lib/doctest.py", line 254, in _load_testfile
    with open(filename, encoding=encoding) as f:
PermissionError: [Errno 13] Permission denied: '/tmp/tmp65cpy_0k'

======================================================================
ERROR: test_lineendings (__main__) [10]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3350, in __main__.test_lineendings
    >>> with open(fn, 'wb') as f:
  File "<doctest __main__.test_lineendings[10]>", line 1, in <module>
    with open(fn, 'wb') as f:
TypeError: 'NoneType' object is not callable

======================================================================
ERROR: test_lineendings (__main__) [11]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3353, in __main__.test_lineendings
    >>> doctest.testfile(fn, module_relative=False, verbose=False)
  File "<doctest __main__.test_lineendings[11]>", line 1, in <module>
    doctest.testfile(fn, module_relative=False, verbose=False)
  File "/pkg/store/python-0/lib/doctest.py", line 2211, in testfile
    text, filename = _load_testfile(filename, package, module_relative,
  File "/pkg/store/python-0/lib/doctest.py", line 254, in _load_testfile
    with open(filename, encoding=encoding) as f:
PermissionError: [Errno 13] Permission denied: '/tmp/tmpxjbl3jmr'

======================================================================
ERROR: test_lineendings (__main__) [20]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3372, in __main__.test_lineendings
    >>> with open(fn, 'wb') as f:
  File "<doctest __main__.test_lineendings[20]>", line 1, in <module>
    with open(fn, 'wb') as f:
TypeError: 'NoneType' object is not callable

======================================================================
ERROR: test_lineendings (__main__) [21]
Doctest: __main__.test_lineendings
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3385, in __main__.test_lineendings
    >>> with test_hook(dn):
PermissionError: [Errno 13] Permission denied: '/tmp/tmp4zsgms5d/doctest_testpkg/doctest_testfile.txt'

======================================================================
ERROR: test_testsource (__main__) [2]
Doctest: __main__.test_testsource
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2135, in __main__.test_testsource
    >>> print(doctest.testsource(test_doctest, name))
  File "<doctest __main__.test_testsource[2]>", line 1, in <module>
    print(doctest.testsource(test_doctest, name))
  File "/pkg/store/python-0/lib/doctest.py", line 2834, in testsource
    raise ValueError(name, "not found in tests")
ValueError: ('test.test_doctest.test_doctest.sample_func', 'not found in tests')

======================================================================
ERROR: test_testsource (__main__) [4]
Doctest: __main__.test_testsource
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2146, in __main__.test_testsource
    >>> print(doctest.testsource(test_doctest, name))
  File "<doctest __main__.test_testsource[4]>", line 1, in <module>
    print(doctest.testsource(test_doctest, name))
  File "/pkg/store/python-0/lib/doctest.py", line 2834, in testsource
    raise ValueError(name, "not found in tests")
ValueError: ('test.test_doctest.test_doctest.SampleNewStyleClass', 'not found in tests')

======================================================================
ERROR: test_testsource (__main__) [6]
Doctest: __main__.test_testsource
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2155, in __main__.test_testsource
    >>> print(doctest.testsource(test_doctest, name))
  File "<doctest __main__.test_testsource[6]>", line 1, in <module>
    print(doctest.testsource(test_doctest, name))
  File "/pkg/store/python-0/lib/doctest.py", line 2834, in testsource
    raise ValueError(name, "not found in tests")
ValueError: ('test.test_doctest.test_doctest.SampleClass.a_classmethod', 'not found in tests')

======================================================================
FAIL: DebugRunner (doctest) [13]
Doctest: doctest.DebugRunner
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 1967, in doctest.DebugRunner
    >>> test.globs
AssertionError: Failed example:
    test.globs
Expected:
    {'x': 1}
Got:
    {'__doc__': None, 'x': 1}

======================================================================
FAIL: DebugRunner (doctest) [17]
Doctest: doctest.DebugRunner
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 1981, in doctest.DebugRunner
    >>> test.globs
AssertionError: Failed example:
    test.globs
Expected:
    {'x': 2}
Got:
    {'__doc__': None, 'x': 2}

======================================================================
FAIL: test_DocFileSuite (__main__) [7]
Doctest: __main__.test_DocFileSuite
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2714, in __main__.test_DocFileSuite
    >>> suite.run(unittest.TestResult())
AssertionError: Failed example:
    suite.run(unittest.TestResult())
Expected:
    <unittest.result.TestResult run=1 errors=1 failures=0>
Got:
    <unittest.result.TestResult run=3 errors=2 failures=0>

======================================================================
FAIL: test_DocFileSuite (__main__) [34]
Doctest: __main__.test_DocFileSuite
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2811, in __main__.test_DocFileSuite
    >>> test_doctest.sillySetup
AssertionError: Failed example:
    test_doctest.sillySetup
Expected:
    Traceback (most recent call last):
    ...
    AttributeError: module 'test.test_doctest.test_doctest' has no attribute 'sillySetup'
Got:
    Traceback (most recent call last):
      File "<doctest __main__.test_DocFileSuite[34]>", line 1, in <module>
        test_doctest.sillySetup
    AttributeError: module '__main__' has no attribute 'sillySetup'

======================================================================
FAIL: test_DocFileSuite_errors (__main__) [5]
Doctest: __main__.test_DocFileSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2870, in __main__.test_DocFileSuite_errors
    >>> print(result.errors[0][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[0][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...test_doctest_errors.txt", line 6, in test_doctest_errors.txt
        >...>> 1/0
      File "<doctest test_doctest_errors.txt[1]>", line 1, in <module>
        1/0
        ~^~
    ZeroDivisionError: division by zero
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test_doctest_errors.txt", line 6, in test_doctest_errors.txt
        >>> 1/0
      File "<doctest test_doctest_errors.txt[1]>", line 1, in <module>
        1/0
    ZeroDivisionError: division by zero
    <BLANKLINE>

======================================================================
FAIL: test_DocFileSuite_errors (__main__) [6]
Doctest: __main__.test_DocFileSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2879, in __main__.test_DocFileSuite_errors
    >>> print(result.errors[1][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[1][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...test_doctest_errors.txt", line 11, in test_doctest_errors.txt
        >...>> f()
      File "<doctest test_doctest_errors.txt[3]>", line 1, in <module>
        f()
        ~^^
      File "<doctest test_doctest_errors.txt[2]>", line 2, in f
        2 + '2'
        ~~^~~~~
    TypeError: ...
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test_doctest_errors.txt", line 11, in test_doctest_errors.txt
        >>> f()
      File "<doctest test_doctest_errors.txt[3]>", line 1, in <module>
        f()
      File "<doctest test_doctest_errors.txt[2]>", line 2, in f
        2 + '2'
    TypeError: unsupported operand type(s) for +: 'int' and 'str'
    <BLANKLINE>

======================================================================
FAIL: test_DocFileSuite_errors (__main__) [7]
Doctest: __main__.test_DocFileSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2891, in __main__.test_DocFileSuite_errors
    >>> print(result.errors[2][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[2][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...test_doctest_errors.txt", line 13, in test_doctest_errors.txt
        >...>> 2+*3
      File "<doctest test_doctest_errors.txt[4]>", line 1
        2+*3
          ^
    SyntaxError: invalid syntax
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test_doctest_errors.txt", line 13, in test_doctest_errors.txt
        >>> 2+*3
      File "<doctest test_doctest_errors.txt[4]>", line 1
        2+*3
          ^^
    SyntaxError: invalid syntax
    <BLANKLINE>

======================================================================
FAIL: basics (__main__.test_DocTestFinder) [48]
Doctest: __main__.test_DocTestFinder.basics
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 679, in __main__.test_DocTestFinder.basics
    >>> for t in tests:
AssertionError: Failed example:
    for t in tests:
        print('%5s  %s' % (t.lineno, t.name))
Expected:
     None  test.test_doctest.doctest_lineno
     None  test.test_doctest.doctest_lineno.ClassWithACachedProperty
      102  test.test_doctest.doctest_lineno.ClassWithACachedProperty.cached
       22  test.test_doctest.doctest_lineno.ClassWithDocstring
       30  test.test_doctest.doctest_lineno.ClassWithDoctest
     None  test.test_doctest.doctest_lineno.ClassWithoutDocstring
     None  test.test_doctest.doctest_lineno.MethodWrapper
       53  test.test_doctest.doctest_lineno.MethodWrapper.classmethod_with_doctest
       39  test.test_doctest.doctest_lineno.MethodWrapper.method_with_docstring
       45  test.test_doctest.doctest_lineno.MethodWrapper.method_with_doctest
     None  test.test_doctest.doctest_lineno.MethodWrapper.method_without_docstring
       61  test.test_doctest.doctest_lineno.MethodWrapper.property_with_doctest
       86  test.test_doctest.doctest_lineno.cached_func_with_doctest
     None  test.test_doctest.doctest_lineno.cached_func_without_docstring
        4  test.test_doctest.doctest_lineno.func_with_docstring
       77  test.test_doctest.doctest_lineno.func_with_docstring_wrapped
       12  test.test_doctest.doctest_lineno.func_with_doctest
     None  test.test_doctest.doctest_lineno.func_without_docstring
Got:
     None  test.test_doctest.doctest_lineno
     None  test.test_doctest.doctest_lineno.ClassWithACachedProperty
      102  test.test_doctest.doctest_lineno.ClassWithACachedProperty.cached
       22  test.test_doctest.doctest_lineno.ClassWithDocstring
       30  test.test_doctest.doctest_lineno.ClassWithDoctest
     None  test.test_doctest.doctest_lineno.ClassWithoutDocstring
     None  test.test_doctest.doctest_lineno.MethodWrapper
       53  test.test_doctest.doctest_lineno.MethodWrapper.classmethod_with_doctest
       39  test.test_doctest.doctest_lineno.MethodWrapper.method_with_docstring
       45  test.test_doctest.doctest_lineno.MethodWrapper.method_with_doctest
     None  test.test_doctest.doctest_lineno.MethodWrapper.method_without_docstring
       61  test.test_doctest.doctest_lineno.MethodWrapper.property_with_doctest
     None  test.test_doctest.doctest_lineno.cached_func_with_doctest
     None  test.test_doctest.doctest_lineno.cached_func_without_docstring
        4  test.test_doctest.doctest_lineno.func_with_docstring
       12  test.test_doctest.doctest_lineno.func_with_doctest
     None  test.test_doctest.doctest_lineno.func_without_docstring

======================================================================
FAIL: exceptions (__main__.test_DocTestRunner) [5]
Doctest: __main__.test_DocTestRunner.exceptions
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 1211, in __main__.test_DocTestRunner.exceptions
    >>> doctest.DocTestRunner(verbose=False).run(test)
AssertionError: Failed example:
    doctest.DocTestRunner(verbose=False).run(test)
Expected:
    TestResults(failed=0, attempted=2)
Got:
    **********************************************************************
    File "/tmp/test_doctest.py", line 4, in f
    Failed example:
        print(x//0)
    Expected:
        Traceback (most recent call last):
        ZeroDivisionError: division by zero
    Got:
        Traceback (most recent call last):
          File "<doctest f[1]>", line 1, in <module>
            print(x//0)
        ZeroDivisionError: integer division or modulo by zero
    TestResults(failed=1, attempted=2)

======================================================================
FAIL: exceptions (__main__.test_DocTestRunner) [8]
Doctest: __main__.test_DocTestRunner.exceptions
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 1228, in __main__.test_DocTestRunner.exceptions
    >>> doctest.DocTestRunner(verbose=False).run(test)
AssertionError: Failed example:
    doctest.DocTestRunner(verbose=False).run(test)
    # doctest: +ELLIPSIS
Expected:
    **********************************************************************
    File ..., line 4, in f
    Failed example:
        print('pre-exception output', x//0)
    Exception raised:
        ...
        ZeroDivisionError: division by zero
    TestResults(failed=1, attempted=2)
Got:
    **********************************************************************
    File "/tmp/test_doctest.py", line 4, in f
    Failed example:
        print('pre-exception output', x//0)
    Exception raised:
        Traceback (most recent call last):
          File "<doctest f[1]>", line 1, in <module>
            print('pre-exception output', x//0)
        ZeroDivisionError: integer division or modulo by zero
    TestResults(failed=1, attempted=2)

======================================================================
FAIL: exceptions (__main__.test_DocTestRunner) [41]
Doctest: __main__.test_DocTestRunner.exceptions
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 1422, in __main__.test_DocTestRunner.exceptions
    >>> doctest.DocTestRunner(verbose=False).run(test)
AssertionError: Failed example:
    doctest.DocTestRunner(verbose=False).run(test)
    # doctest: +ELLIPSIS
Expected:
    **********************************************************************
    File ..., line 3, in f
    Failed example:
        1//0
    Exception raised:
        Traceback (most recent call last):
        ...
        ZeroDivisionError: division by zero
    TestResults(failed=1, attempted=1)
Got:
    **********************************************************************
    File "/tmp/test_doctest.py", line 3, in f
    Failed example:
        1//0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest f[0]>", line 1, in <module>
            1//0
        ZeroDivisionError: integer division or modulo by zero
    TestResults(failed=1, attempted=1)

======================================================================
FAIL: test_DocTestSuite (__main__) [39]
Doctest: __main__.test_DocTestSuite
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2560, in __main__.test_DocTestSuite
    >>> test_doctest.sillySetup
AssertionError: Failed example:
    test_doctest.sillySetup
Expected:
    Traceback (most recent call last):
    ...
    AttributeError: module 'test.test_doctest.test_doctest' has no attribute 'sillySetup'
Got:
    Traceback (most recent call last):
      File "<doctest __main__.test_DocTestSuite[39]>", line 1, in <module>
        test_doctest.sillySetup
    AttributeError: module '__main__' has no attribute 'sillySetup'

======================================================================
FAIL: test_DocTestSuite_errors (__main__) [8]
Doctest: __main__.test_DocTestSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2623, in __main__.test_DocTestSuite_errors
    >>> print(result.errors[0][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[0][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...sample_doctest_errors.py", line 7, in test.test_doctest.sample_doctest_errors
        >...>> 1/0
      File "<doctest test.test_doctest.sample_doctest_errors[1]>", line 1, in <module>
        1/0
        ~^~
    ZeroDivisionError: division by zero
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test/test_doctest/sample_doctest_errors.py", line 7, in test.test_doctest.sample_doctest_errors
        >>> 1/0
      File "<doctest test.test_doctest.sample_doctest_errors[1]>", line 1, in <module>
        1/0
    ZeroDivisionError: division by zero
    <BLANKLINE>

======================================================================
FAIL: test_DocTestSuite_errors (__main__) [9]
Doctest: __main__.test_DocTestSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2632, in __main__.test_DocTestSuite_errors
    >>> print(result.errors[1][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[1][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...sample_doctest_errors.py", line 39, in test.test_doctest.sample_doctest_errors.__test__.bad
        >...>> 1/0
      File "<doctest test.test_doctest.sample_doctest_errors.__test__.bad[1]>", line 1, in <module>
        1/0
        ~^~
    ZeroDivisionError: division by zero
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test/test_doctest/sample_doctest_errors.py", line 39, in test.test_doctest.sample_doctest_errors.__test__.bad
        >>> 1/0
      File "<doctest test.test_doctest.sample_doctest_errors.__test__.bad[1]>", line 1, in <module>
        1/0
    ZeroDivisionError: division by zero
    <BLANKLINE>

======================================================================
FAIL: test_DocTestSuite_errors (__main__) [10]
Doctest: __main__.test_DocTestSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2641, in __main__.test_DocTestSuite_errors
    >>> print(result.errors[2][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[2][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...sample_doctest_errors.py", line 18, in test.test_doctest.sample_doctest_errors.errors
        >...>> 1/0
      File "<doctest test.test_doctest.sample_doctest_errors.errors[1]>", line 1, in <module>
        1/0
        ~^~
    ZeroDivisionError: division by zero
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test/test_doctest/sample_doctest_errors.py", line 18, in test.test_doctest.sample_doctest_errors.errors
        >>> 1/0
      File "<doctest test.test_doctest.sample_doctest_errors.errors[1]>", line 1, in <module>
        1/0
    ZeroDivisionError: division by zero
    <BLANKLINE>

======================================================================
FAIL: test_DocTestSuite_errors (__main__) [11]
Doctest: __main__.test_DocTestSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2650, in __main__.test_DocTestSuite_errors
    >>> print(result.errors[3][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[3][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...sample_doctest_errors.py", line 23, in test.test_doctest.sample_doctest_errors.errors
        >...>> f()
      File "<doctest test.test_doctest.sample_doctest_errors.errors[3]>", line 1, in <module>
        f()
        ~^^
      File "<doctest test.test_doctest.sample_doctest_errors.errors[2]>", line 2, in f
        2 + '2'
        ~~^~~~~
    TypeError: ...
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test/test_doctest/sample_doctest_errors.py", line 23, in test.test_doctest.sample_doctest_errors.errors
        >>> f()
      File "<doctest test.test_doctest.sample_doctest_errors.errors[3]>", line 1, in <module>
        f()
      File "<doctest test.test_doctest.sample_doctest_errors.errors[2]>", line 2, in f
        2 + '2'
    TypeError: unsupported operand type(s) for +: 'int' and 'str'
    <BLANKLINE>

======================================================================
FAIL: test_DocTestSuite_errors (__main__) [12]
Doctest: __main__.test_DocTestSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2662, in __main__.test_DocTestSuite_errors
    >>> print(result.errors[4][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[4][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...sample_doctest_errors.py", line 25, in test.test_doctest.sample_doctest_errors.errors
        >...>> g()
      File "<doctest test.test_doctest.sample_doctest_errors.errors[4]>", line 1, in <module>
        g()
        ~^^
      File "...sample_doctest_errors.py", line 12, in g
        [][0] # line 12
        ~~^^^
    IndexError: list index out of range
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test/test_doctest/sample_doctest_errors.py", line 25, in test.test_doctest.sample_doctest_errors.errors
        >>> g()
      File "<doctest test.test_doctest.sample_doctest_errors.errors[4]>", line 1, in <module>
        g()
      File "/tmp/test/test_doctest/sample_doctest_errors.py", line 12, in g
        [][0] # line 12
    IndexError: list index out of range
    <BLANKLINE>

======================================================================
FAIL: test_DocTestSuite_errors (__main__) [13]
Doctest: __main__.test_DocTestSuite_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2674, in __main__.test_DocTestSuite_errors
    >>> print(result.errors[5][1]) # doctest: +ELLIPSIS
AssertionError: Failed example:
    print(result.errors[5][1]) # doctest: +ELLIPSIS
Expected:
    Traceback (most recent call last):
      File "...sample_doctest_errors.py", line 31, in test.test_doctest.sample_doctest_errors.syntax_error
        >...>> 2+*3
      File "<doctest test.test_doctest.sample_doctest_errors.syntax_error[0]>", line 1
        2+*3
          ^
    SyntaxError: invalid syntax
    <BLANKLINE>
Got:
    Traceback (most recent call last):
      File "/tmp/test/test_doctest/sample_doctest_errors.py", line 31, in test.test_doctest.sample_doctest_errors.syntax_error
        >>> 2+*3
      File "<doctest test.test_doctest.sample_doctest_errors.syntax_error[0]>", line 1
        2+*3
          ^^
    SyntaxError: invalid syntax
    <BLANKLINE>

======================================================================
FAIL: test_pdb_set_trace (__main__) [16]
Doctest: __main__.test_pdb_set_trace
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2260, in __main__.test_pdb_set_trace
    >>> try:
AssertionError: Failed example:
    try:
        runner.run(test)
    finally:
        sys.stdin = real_stdin
Expected:
    > <doctest test.test_doctest.test_doctest.test_pdb_set_trace[11]>(3)calls_set_trace()
    -> import pdb; pdb.set_trace()
    (Pdb) print(y)
    2
    (Pdb) up
    > <doctest foo-bar@baz[1]>(1)<module>()
    -> calls_set_trace()
    (Pdb) print(x)
    1
    (Pdb) continue
    TestResults(failed=0, attempted=2)
Got:
    > <doctest __main__.test_pdb_set_trace[11]>(3)calls_set_trace()
    -> import pdb; pdb.set_trace()
    (Pdb) print(y)
    2
    (Pdb) up
    > <doctest foo-bar@baz[1]>(1)<module>()
    -> calls_set_trace()
    (Pdb) print(x)
    1
    (Pdb) continue
    TestResults(failed=0, attempted=2)

======================================================================
FAIL: test_pdb_set_trace_nested (__main__) [9]
Doctest: __main__.test_pdb_set_trace_nested
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 2378, in __main__.test_pdb_set_trace_nested
    >>> try:
AssertionError: Failed example:
    try:
        runner.run(test)
    finally:
        sys.stdin = real_stdin
    # doctest: +REPORT_NDIFF
Differences (ndiff with -expected +actual):
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(4)calls_set_trace()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(4)calls_set_trace()
    ?              ^^^^^^
      -> import pdb; pdb.set_trace()
      (Pdb) step
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(5)calls_set_trace()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(5)calls_set_trace()
    ?              ^^^^^^
      -> self.f1()
      (Pdb) print(y)
      1
      (Pdb) step
      --Call--
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(7)f1()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(7)f1()
    ?              ^^^^^^
      -> def f1(self):
      (Pdb) step
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(8)f1()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(8)f1()
    ?              ^^^^^^
      -> x = 1
      (Pdb) step
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(9)f1()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(9)f1()
    ?              ^^^^^^
      -> self.f2()
      (Pdb) step
      --Call--
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(11)f2()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(11)f2()
    ?              ^^^^^^
      -> def f2(self):
      (Pdb) step
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(12)f2()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(12)f2()
    ?              ^^^^^^
      -> z = 1
      (Pdb) step
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(13)f2()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(13)f2()
    ?              ^^^^^^
      -> z = 2
      (Pdb) print(z)
      1
      (Pdb) up
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(9)f1()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(9)f1()
    ?              ^^^^^^
      -> self.f2()
      (Pdb) print(x)
      1
      (Pdb) up
    - > <doctest test.test_doctest.test_doctest.test_pdb_set_trace_nested[0]>(5)calls_set_trace()
    ?            --------- ------------ ^^^^^^^
    + > <doctest __main__.test_pdb_set_trace_nested[0]>(5)calls_set_trace()
    ?              ^^^^^^
      -> self.f1()
      (Pdb) print(y)
      1
      (Pdb) up
      > <doctest foo-bar@baz[1]>(1)<module>()
      -> calls_set_trace()
      (Pdb) print(foo)
      *** NameError: name 'foo' is not defined
      (Pdb) continue
      TestResults(failed=0, attempted=2)

======================================================================
FAIL: test_testfile_errors (__main__) [0]
Doctest: __main__.test_testfile_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3225, in __main__.test_testfile_errors
    >>> doctest.testfile('test_doctest_errors.txt', verbose=False) # doctest: +ELLIPSIS
AssertionError: Failed example:
    doctest.testfile('test_doctest_errors.txt', verbose=False) # doctest: +ELLIPSIS
Expected:
    **********************************************************************
    File "...test_doctest_errors.txt", line 4, in test_doctest_errors.txt
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "...test_doctest_errors.txt", line 6, in test_doctest_errors.txt
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test_doctest_errors.txt[1]>", line 1, in <module>
            1/0
            ~^~
        ZeroDivisionError: division by zero
    **********************************************************************
    File "...test_doctest_errors.txt", line 11, in test_doctest_errors.txt
    Failed example:
        f()
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test_doctest_errors.txt[3]>", line 1, in <module>
            f()
            ~^^
          File "<doctest test_doctest_errors.txt[2]>", line 2, in f
            2 + '2'
            ~~^~~~~
        TypeError: ...
    **********************************************************************
    File "...test_doctest_errors.txt", line 13, in test_doctest_errors.txt
    Failed example:
        2+*3
    Exception raised:
          File "<doctest test_doctest_errors.txt[4]>", line 1
            2+*3
              ^
        SyntaxError: invalid syntax
    **********************************************************************
    1 item had failures:
       4 of   5 in test_doctest_errors.txt
    ***Test Failed*** 4 failures.
    TestResults(failed=4, attempted=5)
Got:
    **********************************************************************
    File "/tmp/test_doctest_errors.txt", line 4, in test_doctest_errors.txt
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "/tmp/test_doctest_errors.txt", line 6, in test_doctest_errors.txt
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test_doctest_errors.txt[1]>", line 1, in <module>
            1/0
        ZeroDivisionError: division by zero
    **********************************************************************
    File "/tmp/test_doctest_errors.txt", line 11, in test_doctest_errors.txt
    Failed example:
        f()
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test_doctest_errors.txt[3]>", line 1, in <module>
            f()
          File "<doctest test_doctest_errors.txt[2]>", line 2, in f
            2 + '2'
        TypeError: unsupported operand type(s) for +: 'int' and 'str'
    **********************************************************************
    File "/tmp/test_doctest_errors.txt", line 13, in test_doctest_errors.txt
    Failed example:
        2+*3
    Exception raised:
          File "<doctest test_doctest_errors.txt[4]>", line 1
            2+*3
              ^^
        SyntaxError: invalid syntax
    **********************************************************************
    1 item had failures:
       4 of   5 in test_doctest_errors.txt
    ***Test Failed*** 4 failures.
    TestResults(failed=4, attempted=5)

======================================================================
FAIL: test_testmod_errors (__main__) [1]
Doctest: __main__.test_testmod_errors
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_doctest.py", line 3407, in __main__.test_testmod_errors
    >>> doctest.testmod(mod, verbose=False) # doctest: +ELLIPSIS
AssertionError: Failed example:
    doctest.testmod(mod, verbose=False) # doctest: +ELLIPSIS
Expected:
    **********************************************************************
    File "...sample_doctest_errors.py", line 5, in test.test_doctest.sample_doctest_errors
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "...sample_doctest_errors.py", line 7, in test.test_doctest.sample_doctest_errors
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors[1]>", line 1, in <module>
            1/0
            ~^~
        ZeroDivisionError: division by zero
    **********************************************************************
    File "...sample_doctest_errors.py", line 37, in test.test_doctest.sample_doctest_errors.__test__.bad
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "...sample_doctest_errors.py", line 39, in test.test_doctest.sample_doctest_errors.__test__.bad
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.__test__.bad[1]>", line 1, in <module>
            1/0
            ~^~
        ZeroDivisionError: division by zero
    **********************************************************************
    File "...sample_doctest_errors.py", line 16, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "...sample_doctest_errors.py", line 18, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.errors[1]>", line 1, in <module>
            1/0
            ~^~
        ZeroDivisionError: division by zero
    **********************************************************************
    File "...sample_doctest_errors.py", line 23, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        f()
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.errors[3]>", line 1, in <module>
            f()
            ~^^
          File "<doctest test.test_doctest.sample_doctest_errors.errors[2]>", line 2, in f
            2 + '2'
            ~~^~~~~
        TypeError: ...
    **********************************************************************
    File "...sample_doctest_errors.py", line 25, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        g()
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.errors[4]>", line 1, in <module>
            g()
            ~^^
          File "...sample_doctest_errors.py", line 12, in g
            [][0] # line 12
            ~~^^^
        IndexError: list index out of range
    **********************************************************************
    File "...sample_doctest_errors.py", line 31, in test.test_doctest.sample_doctest_errors.syntax_error
    Failed example:
        2+*3
    Exception raised:
          File "<doctest test.test_doctest.sample_doctest_errors.syntax_error[0]>", line 1
            2+*3
              ^
        SyntaxError: invalid syntax
    **********************************************************************
    4 items had failures:
       2 of   2 in test.test_doctest.sample_doctest_errors
       2 of   2 in test.test_doctest.sample_doctest_errors.__test__.bad
       4 of   5 in test.test_doctest.sample_doctest_errors.errors
       1 of   1 in test.test_doctest.sample_doctest_errors.syntax_error
    ***Test Failed*** 9 failures.
    TestResults(failed=9, attempted=10)
Got:
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 5, in test.test_doctest.sample_doctest_errors
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 7, in test.test_doctest.sample_doctest_errors
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors[1]>", line 1, in <module>
            1/0
        ZeroDivisionError: division by zero
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 37, in test.test_doctest.sample_doctest_errors.__test__.bad
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 39, in test.test_doctest.sample_doctest_errors.__test__.bad
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.__test__.bad[1]>", line 1, in <module>
            1/0
        ZeroDivisionError: division by zero
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 16, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        2 + 2
    Expected:
        5
    Got:
        4
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 18, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        1/0
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.errors[1]>", line 1, in <module>
            1/0
        ZeroDivisionError: division by zero
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 23, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        f()
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.errors[3]>", line 1, in <module>
            f()
          File "<doctest test.test_doctest.sample_doctest_errors.errors[2]>", line 2, in f
            2 + '2'
        TypeError: unsupported operand type(s) for +: 'int' and 'str'
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 25, in test.test_doctest.sample_doctest_errors.errors
    Failed example:
        g()
    Exception raised:
        Traceback (most recent call last):
          File "<doctest test.test_doctest.sample_doctest_errors.errors[4]>", line 1, in <module>
            g()
          File "/tmp/test/test_doctest/sample_doctest_errors.py", line 12, in g
            [][0] # line 12
        IndexError: list index out of range
    **********************************************************************
    File "/tmp/test/test_doctest/sample_doctest_errors.py", line 31, in test.test_doctest.sample_doctest_errors.syntax_error
    Failed example:
        2+*3
    Exception raised:
          File "<doctest test.test_doctest.sample_doctest_errors.syntax_error[0]>", line 1
            2+*3
              ^^
        SyntaxError: invalid syntax
    **********************************************************************
    4 items had failures:
       2 of   2 in test.test_doctest.sample_doctest_errors
       2 of   2 in test.test_doctest.sample_doctest_errors.__test__.bad
       4 of   5 in test.test_doctest.sample_doctest_errors.errors
       1 of   1 in test.test_doctest.sample_doctest_errors.syntax_error
    ***Test Failed*** 9 failures.
    TestResults(failed=9, attempted=10)

----------------------------------------------------------------------
Ran 78 tests in Ns

FAILED (failures=22, errors=12, skipped=4)
