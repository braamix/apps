--- unittest ---
error TestCause.testCauseSyntax: AttributeError: 'ValueError' object has no attribute '__suppress_context__'
fail TestCause.test_class_cause: AssertionError: <class 'KeyError'> is not an instance of <class 'KeyError'>
error TestCause.test_class_cause_nonexception_result: IndexError
error TestCause.test_erroneous_cause: IndexError
ok TestCause.test_instance_cause
error TestCause.test_invalid_cause: IndexError
ok TestContext.test_3118
error TestContext.test_3611: AttributeError: 'module' object has no attribute 'catch_unraisable_exception'
ok TestContext.test_c_exception_context
ok TestContext.test_c_exception_raise
ok TestContext.test_class_context_class_raise
ok TestContext.test_class_context_instance_raise
fail TestContext.test_context_manager: AssertionError: IndexError() is not an instance of <class 'ZeroDivisionError'>
fail TestContext.test_cycle_broken: AssertionError: IndexError() is not None
ok TestContext.test_instance_context_instance_raise
fail TestContext.test_noraise_finally: AssertionError: IndexError() is not None
ok TestContext.test_not_last
fail TestContext.test_raise_finally: AssertionError: IndexError() is not an instance of <class 'ZeroDivisionError'>
fail TestContext.test_reraise_cycle_broken: AssertionError: IndexError() is not None
ok TestRaise.test_assert_with_tuple_arg
error TestRaise.test_erroneous_exception: MyException
ok TestRaise.test_except_reraise
error TestRaise.test_finally_reraise: TypeError: foo
error TestRaise.test_invalid_reraise: IndexError
ok TestRaise.test_nested_reraise
error TestRaise.test_new_returns_invalid_instance: MyException
ok TestRaise.test_raise_from_None
ok TestRaise.test_reraise
ok TestRaise.test_with_reraise1
ok TestRaise.test_with_reraise2
ok TestRaise.test_yield_reraise
ok TestRemovedFunctionality.test_strings
ok TestRemovedFunctionality.test_tuples
error TestTraceback.test_accepts_traceback: AttributeError: 'OSError' object has no attribute '__traceback__'
error TestTraceback.test_sets_traceback: AttributeError: 'IndexError' object has no attribute '__traceback__'
error TestTracebackType.test_attrs: AttributeError: 'ValueError' object has no attribute '__traceback__'
error TestTracebackType.test_constructor: AttributeError: 'OSError' object has no attribute '__traceback__'
--- ran 37 ok 18 fail 6 error 13 skip 0 ---
