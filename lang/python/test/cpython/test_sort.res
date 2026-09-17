--- unittest ---
error TestBase.testStressfully: TypeError: '<' not supported between instances of 'Stable' and 'Stable'
ok TestBase.test_small_stability
error TestBugs.test_bug453523: TypeError: '<' not supported between instances of 'int' and 'C'
fail TestBugs.test_undetected_mutation: AssertionError: ValueError not raised
ok TestDecorateSortUndecorate.test_baddecorator
ok TestDecorateSortUndecorate.test_decorated
ok TestDecorateSortUndecorate.test_key_with_exception
fail TestDecorateSortUndecorate.test_key_with_mutating_del: AssertionError: ValueError not raised
ok TestDecorateSortUndecorate.test_key_with_mutating_del_and_exception
fail TestDecorateSortUndecorate.test_key_with_mutation: AssertionError: ValueError not raised
ok TestDecorateSortUndecorate.test_reverse
ok TestDecorateSortUndecorate.test_reverse_stability
ok TestDecorateSortUndecorate.test_stability
ok TestOptimizedCompares.test_none_in_tuples
ok TestOptimizedCompares.test_not_all_tuples
ok TestOptimizedCompares.test_safe_object_compare
ok TestOptimizedCompares.test_unsafe_float_compare
ok TestOptimizedCompares.test_unsafe_latin_compare
ok TestOptimizedCompares.test_unsafe_long_compare
fail TestOptimizedCompares.test_unsafe_object_compare: AssertionError: ValueError not raised
ok TestOptimizedCompares.test_unsafe_tuple_compare
--- ran 21 ok 15 fail 4 error 2 skip 0 ---
