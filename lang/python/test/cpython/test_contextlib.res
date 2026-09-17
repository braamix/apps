--- unittest ---
ok ClosingTestCase.test_closing
ok ClosingTestCase.test_closing_error
skip ClosingTestCase.test_instance_docs: test requires docstrings
ok ContextManagerTestCase.test_contextmanager_attribs
ok ContextManagerTestCase.test_contextmanager_do_not_unchain_non_stopiteration_exceptions
skip ContextManagerTestCase.test_contextmanager_doc_attrib: test requires docstrings
ok ContextManagerTestCase.test_contextmanager_except
fail ContextManagerTestCase.test_contextmanager_except_pep479: AssertionError: RuntimeError('generator raised StopIteration') is not StopIteration('spam')
fail ContextManagerTestCase.test_contextmanager_except_stopiter: 2 subtests: type=<class 'StopIteration'>: AssertionError: RuntimeError('generator raised StopIteration') is not StopIteration('spam'); type=<class '__main__.ContextManagerTestCase.test_contextmanager_except_stopiter.<locals>.StopIterationSubclass'>: AssertionError: RuntimeError('generator raised StopIteration') is not StopIterationSubclass('spam')
ok ContextManagerTestCase.test_contextmanager_finally
ok ContextManagerTestCase.test_contextmanager_no_reraise
ok ContextManagerTestCase.test_contextmanager_non_normalised
ok ContextManagerTestCase.test_contextmanager_plain
fail ContextManagerTestCase.test_contextmanager_traceback: AssertionError: 0 != 1
ok ContextManagerTestCase.test_contextmanager_trap_no_yield
ok ContextManagerTestCase.test_contextmanager_trap_second_yield
ok ContextManagerTestCase.test_contextmanager_trap_yield_after_throw
ok ContextManagerTestCase.test_contextmanager_wrap_runtimeerror
skip ContextManagerTestCase.test_instance_docstring_given_cm_docstring: test requires docstrings
ok ContextManagerTestCase.test_keywords
ok ContextManagerTestCase.test_nokeepref
ok ContextManagerTestCase.test_param_errors
error ContextManagerTestCase.test_recursive: ModuleNotFoundError: No module named 'inspect'
ok FileContextTestCase.testWithOpen
ok LockContextTestCase.testWithBoundedSemaphore
ok LockContextTestCase.testWithCondition
ok LockContextTestCase.testWithLock
ok LockContextTestCase.testWithRLock
ok LockContextTestCase.testWithSemaphore
ok NullcontextTestCase.test_nullcontext
ok TestAbstractContextManager.test_enter
ok TestAbstractContextManager.test_exit_is_abstract
ok TestAbstractContextManager.test_slots
ok TestAbstractContextManager.test_structural_subclassing
error TestChdir.test_exception: FileNotFoundError: [Errno 2] No such file or directory: '/tmp/data'
error TestChdir.test_reentrant: FileNotFoundError: [Errno 2] No such file or directory: '/tmp/data'
error TestChdir.test_simple: FileNotFoundError: [Errno 2] No such file or directory: '/tmp/data'
ok TestContextDecorator.test_contextdecorator
error TestContextDecorator.test_contextdecorator_as_mixin: ModuleNotFoundError: No module named 'inspect'
ok TestContextDecorator.test_contextdecorator_with_exception
error TestContextDecorator.test_contextmanager_as_decorator: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_contextmanager_decorate_asyncgen_function: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_contextmanager_decorate_coroutine_function: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_contextmanager_decorate_generator_function: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_contextmanager_decorate_generator_function_early_stop: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_contextmanager_decorate_generator_function_exception: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_contextmanager_decorate_generator_function_send_throw: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_decorating_method: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_decorator: ModuleNotFoundError: No module named 'inspect'
error TestContextDecorator.test_decorator_with_exception: ModuleNotFoundError: No module named 'inspect'
skip TestContextDecorator.test_instance_docs: test requires docstrings
error TestContextDecorator.test_typo_enter: AttributeError: 'mycontext' object has no attribute '__enter__'
error TestContextDecorator.test_typo_exit: AttributeError: 'mycontext' object has no attribute '__exit__'
error TestExitStack.test_body_exception_suppress: ModuleNotFoundError: No module named 'inspect'
ok TestExitStack.test_callback
ok TestExitStack.test_close
error TestExitStack.test_dont_reraise_RuntimeError: ModuleNotFoundError: No module named 'inspect'
error TestExitStack.test_enter_context: ModuleNotFoundError: No module named 'inspect'
error TestExitStack.test_enter_context_classmethod: ModuleNotFoundError: No module named 'inspect'
error TestExitStack.test_enter_context_errors: ModuleNotFoundError: No module named 'inspect'
error TestExitStack.test_enter_context_slots: ModuleNotFoundError: No module named 'inspect'
error TestExitStack.test_enter_context_staticmethod: ModuleNotFoundError: No module named 'inspect'
ok TestExitStack.test_excessive_nesting
fail TestExitStack.test_exit_exception_chaining: AssertionError: None is not an instance of <class 'KeyError'>
fail TestExitStack.test_exit_exception_chaining_reference: AssertionError: None is not an instance of <class 'KeyError'>
error TestExitStack.test_exit_exception_chaining_suppress: ModuleNotFoundError: No module named 'inspect'
fail TestExitStack.test_exit_exception_explicit_none_context: 1 subtests: [subtest]: ModuleNotFoundError: No module named 'inspect'
ok TestExitStack.test_exit_exception_non_suppressing
fail TestExitStack.test_exit_exception_traceback: AssertionError: None is not an instance of <class 'ZeroDivisionError'>
fail TestExitStack.test_exit_exception_with_correct_context: AssertionError: ModuleNotFoundError("No module named 'inspect'", name='inspect') is not Exception(4)
fail TestExitStack.test_exit_exception_with_existing_context: AssertionError: None is not Exception(4)
error TestExitStack.test_exit_raise: ModuleNotFoundError: No module named 'inspect'
error TestExitStack.test_exit_suppress: ModuleNotFoundError: No module named 'inspect'
error TestExitStack.test_instance_bypass: ModuleNotFoundError: No module named 'inspect'
skip TestExitStack.test_instance_docs: test requires docstrings
ok TestExitStack.test_no_resources
ok TestExitStack.test_pop_all
error TestExitStack.test_push: ModuleNotFoundError: No module named 'inspect'
ok TestRedirectStderr.test_cm_is_reentrant
ok TestRedirectStderr.test_cm_is_reusable
ok TestRedirectStderr.test_enter_result_is_target
skip TestRedirectStderr.test_instance_docs: test requires docstrings
ok TestRedirectStderr.test_no_redirect_in_init
ok TestRedirectStderr.test_redirect_to_string_io
ok TestRedirectStdout.test_cm_is_reentrant
ok TestRedirectStdout.test_cm_is_reusable
ok TestRedirectStdout.test_enter_result_is_target
skip TestRedirectStdout.test_instance_docs: test requires docstrings
ok TestRedirectStdout.test_no_redirect_in_init
ok TestRedirectStdout.test_redirect_to_string_io
ok TestSuppress.test_cm_is_reentrant
ok TestSuppress.test_exact_exception
ok TestSuppress.test_exception_groups
ok TestSuppress.test_exception_hierarchy
skip TestSuppress.test_instance_docs: test requires docstrings
ok TestSuppress.test_multiple_exception_args
ok TestSuppress.test_no_args
ok TestSuppress.test_no_exception
ok TestSuppress.test_no_result_from_enter
ok TestSuppress.test_other_exception
--- ran 100 ok 54 fail 9 error 29 skip 8 ---
