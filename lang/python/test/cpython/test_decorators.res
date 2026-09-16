--- unittest ---
ok TestClassDecorators.test_double
ok TestClassDecorators.test_order
ok TestClassDecorators.test_simple
error TestDecorators.test_argforms: AttributeError: object has no attribute: dbval
ok TestDecorators.test_bound_function_inside_classmethod
error TestDecorators.test_classmethod: AttributeError: 'classmethod' object has no attribute '__func__'
error TestDecorators.test_dbcheck: NameError: name 'compile' is not defined
error TestDecorators.test_dotted: AttributeError: 'function' object has no attribute '__dict__'
error TestDecorators.test_double: AttributeError: 'function' object has no attribute '__dict__'
error TestDecorators.test_errors: NameError: name 'compile' is not defined
error TestDecorators.test_eval_order: AttributeError: 'NoneType' object has no attribute 'make_decorator'
error TestDecorators.test_expressions: NameError: name 'compile' is not defined
error TestDecorators.test_memoize: AttributeError: 'function' object has no attribute '__name__'
ok TestDecorators.test_order
ok TestDecorators.test_single
error TestDecorators.test_staticmethod: AttributeError: 'staticmethod' object has no attribute '__func__'
--- ran 16 ok 6 fail 0 error 10 skip 0 ---
