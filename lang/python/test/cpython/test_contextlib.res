..s..s.FFF....F....s...............EEE.............sEE...E......FF.E.FFF...s......s.....s......s.....
======================================================================
ERROR: test_exception (__main__.TestChdir.test_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1574, in test_exception
    with chdir(target):
  File "/pkg/store/python-0/lib/contextlib.py", line 854, in __enter__
    os.chdir(self.path)
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/data'

======================================================================
ERROR: test_reentrant (__main__.TestChdir.test_reentrant)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1558, in test_reentrant
    with chdir1:
  File "/pkg/store/python-0/lib/contextlib.py", line 854, in __enter__
    os.chdir(self.path)
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/data'

======================================================================
ERROR: test_simple (__main__.TestChdir.test_simple)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1547, in test_simple
    with chdir(target):
  File "/pkg/store/python-0/lib/contextlib.py", line 854, in __enter__
    os.chdir(self.path)
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/data'

======================================================================
ERROR: test_typo_enter (__main__.TestContextDecorator.test_typo_enter)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 622, in test_typo_enter
    with mycontext():
AttributeError: 'mycontext' object has no attribute '__enter__'. Did you mean '.__unter__' instead of '.__enter__'?

======================================================================
ERROR: test_typo_exit (__main__.TestContextDecorator.test_typo_exit)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 634, in test_typo_exit
    with mycontext():
AttributeError: 'mycontext' object has no attribute '__exit__'. Did you mean '.__uxit__' instead of '.__exit__'?

======================================================================
ERROR: test_dont_reraise_RuntimeError (__main__.TestExitStack.test_dont_reraise_RuntimeError)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1349, in test_dont_reraise_RuntimeError
    es_ctx.enter_context(second())
  File "/pkg/store/python-0/lib/contextlib.py", line 585, in enter_context
    raise TypeError(f"'{cls.__module__}.{cls.__qualname__}' object does "
TypeError: 'contextlib._GeneratorContextManager' object does not support the context manager protocol

======================================================================
ERROR: test_exit_exception_explicit_none_context (__main__.TestExitStack.test_exit_exception_explicit_none_context) (<subtest>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1200, in test_exit_exception_explicit_none_context
    with cm():
  File "/pkg/store/python-0/lib/contextlib.py", line 203, in __enter__
    for once in self.gen:
  File "/tmp/test_contextlib.py", line 1194, in my_cm_with_exit_stack
    stack.enter_context(my_cm())
  File "/pkg/store/python-0/lib/contextlib.py", line 585, in enter_context
    raise TypeError(f"'{cls.__module__}.{cls.__qualname__}' object does "
TypeError: 'contextlib._GeneratorContextManager' object does not support the context manager protocol

======================================================================
FAIL: test_contextmanager_except_pep479 (__main__.ContextManagerTestCase.test_contextmanager_except_pep479)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<string>", line 5, in woohoo
  File "/tmp/test_contextlib.py", line 261, in test_contextmanager_except_pep479
    raise stop_exc
StopIteration: spam

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 260, in test_contextmanager_except_pep479
    with woohoo():
  File "/pkg/store/python-0/lib/contextlib.py", line 222, in __exit__
    self.gen.throw(value)
RuntimeError: generator raised StopIteration

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 263, in test_contextmanager_except_pep479
    self.assertIs(ex, stop_exc)
AssertionError: RuntimeError('generator raised StopIteration') is not StopIteration('spam')

======================================================================
FAIL: test_contextmanager_except_stopiter (__main__.ContextManagerTestCase.test_contextmanager_except_stopiter) (type=<class 'StopIteration'>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 231, in woohoo
    yield
  File "/tmp/test_contextlib.py", line 240, in test_contextmanager_except_stopiter
    raise stop_exc
StopIteration: spam

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 239, in test_contextmanager_except_stopiter
    with woohoo():
  File "/pkg/store/python-0/lib/contextlib.py", line 222, in __exit__
    self.gen.throw(value)
RuntimeError: generator raised StopIteration

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 242, in test_contextmanager_except_stopiter
    self.assertIs(ex, stop_exc)
AssertionError: RuntimeError('generator raised StopIteration') is not StopIteration('spam')

======================================================================
FAIL: test_contextmanager_except_stopiter (__main__.ContextManagerTestCase.test_contextmanager_except_stopiter) (type=<class '__main__.ContextManagerTestCase.test_contextmanager_except_stopiter.<locals>.StopIterationSubclass'>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 231, in woohoo
    yield
  File "/tmp/test_contextlib.py", line 240, in test_contextmanager_except_stopiter
    raise stop_exc
ContextManagerTestCase.test_contextmanager_except_stopiter.<locals>.StopIterationSubclass: spam

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 239, in test_contextmanager_except_stopiter
    with woohoo():
  File "/pkg/store/python-0/lib/contextlib.py", line 222, in __exit__
    self.gen.throw(value)
RuntimeError: generator raised StopIteration

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 242, in test_contextmanager_except_stopiter
    self.assertIs(ex, stop_exc)
AssertionError: RuntimeError('generator raised StopIteration') is not StopIterationSubclass('spam')

======================================================================
FAIL: test_contextmanager_traceback (__main__.ContextManagerTestCase.test_contextmanager_traceback)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 113, in test_contextmanager_traceback
    self.assertEqual(len(frames), 1)
AssertionError: 0 != 1

======================================================================
FAIL: test_exit_exception_chaining (__main__.TestExitStack.test_exit_exception_chaining)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1154, in test_exit_exception_chaining
    with self.exit_stack() as stack:
  File "/pkg/store/python-0/lib/contextlib.py", line 676, in __exit__
    raise exc
  File "/pkg/store/python-0/lib/contextlib.py", line 661, in __exit__
    if cb(*exc_details):
  File "/pkg/store/python-0/lib/contextlib.py", line 552, in _exit_wrapper
    callback(*args, **kwds)
  File "/tmp/test_contextlib.py", line 1145, in raise_exc
    raise exc
IndexError

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1162, in test_exit_exception_chaining
    self.assertIsInstance(exc.__context__, KeyError)
AssertionError: None is not an instance of <class 'KeyError'>

======================================================================
FAIL: test_exit_exception_chaining_reference (__main__.TestExitStack.test_exit_exception_chaining_reference)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1125, in test_exit_exception_chaining_reference
    with RaiseExc(IndexError):
  File "/tmp/test_contextlib.py", line 1103, in __exit__
    raise self.exc
IndexError

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1131, in test_exit_exception_chaining_reference
    self.assertIsInstance(exc.__context__, KeyError)
AssertionError: None is not an instance of <class 'KeyError'>

======================================================================
FAIL: test_exit_exception_traceback (__main__.TestExitStack.test_exit_exception_traceback)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1089, in test_exit_exception_traceback
    self.assertIsInstance(exc.__context__, ZeroDivisionError)
AssertionError: None is not an instance of <class 'ZeroDivisionError'>

======================================================================
FAIL: test_exit_exception_with_correct_context (__main__.TestExitStack.test_exit_exception_with_correct_context)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1253, in test_exit_exception_with_correct_context
    stack.enter_context(gets_the_context_right(exc4))
  File "/pkg/store/python-0/lib/contextlib.py", line 585, in enter_context
    raise TypeError(f"'{cls.__module__}.{cls.__qualname__}' object does "
TypeError: 'contextlib._GeneratorContextManager' object does not support the context manager protocol

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1258, in test_exit_exception_with_correct_context
    self.assertIs(exc, exc4)
AssertionError: TypeError("'contextlib._GeneratorContextManager' object does not support the context manager protocol") is not Exception(4)

======================================================================
FAIL: test_exit_exception_with_existing_context (__main__.TestExitStack.test_exit_exception_with_existing_context)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1279, in test_exit_exception_with_existing_context
    with self.exit_stack() as stack:
  File "/pkg/store/python-0/lib/contextlib.py", line 676, in __exit__
    raise exc
  File "/pkg/store/python-0/lib/contextlib.py", line 661, in __exit__
    if cb(*exc_details):
  File "/pkg/store/python-0/lib/contextlib.py", line 552, in _exit_wrapper
    callback(*args, **kwds)
  File "/tmp/test_contextlib.py", line 1272, in raise_nested
    raise outer_exc
Exception: 5

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_contextlib.py", line 1285, in test_exit_exception_with_existing_context
    self.assertIs(exc.__context__, exc4)
AssertionError: None is not Exception(4)

----------------------------------------------------------------------
Ran 100 tests in Ns

FAILED (failures=9, errors=7, skipped=8)
