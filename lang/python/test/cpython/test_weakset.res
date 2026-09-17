--- unittest ---
ok TestWeakSet.test_abc
ok TestWeakSet.test_add
ok TestWeakSet.test_and
ok TestWeakSet.test_clear
ok TestWeakSet.test_constructor_identity
ok TestWeakSet.test_contains
ok TestWeakSet.test_copy
error TestWeakSet.test_copying: TypeError: cannot pickle 'set' object
error TestWeakSet.test_difference: TypeError: 'generator' object is not an iterator
error TestWeakSet.test_difference_update: TypeError: 'generator' object is not an iterator
ok TestWeakSet.test_discard
ok TestWeakSet.test_eq
ok TestWeakSet.test_gc
fail TestWeakSet.test_gt: AssertionError: False is not true
ok TestWeakSet.test_hash
error TestWeakSet.test_iand: TypeError: 'generator' object is not an iterator
ok TestWeakSet.test_init
error TestWeakSet.test_inplace_on_self: TypeError: 'generator' object is not an iterator
fail TestWeakSet.test_intersection: AssertionError: False != True
error TestWeakSet.test_intersection_update: TypeError: 'generator' object is not an iterator
ok TestWeakSet.test_ior
fail TestWeakSet.test_isdisjoint: AssertionError: False is not true
error TestWeakSet.test_isub: TypeError: 'generator' object is not an iterator
error TestWeakSet.test_ixor: TypeError: 'generator' object is not an iterator
ok TestWeakSet.test_len
ok TestWeakSet.test_len_cycles
ok TestWeakSet.test_len_race
fail TestWeakSet.test_lt: AssertionError: False is not true
ok TestWeakSet.test_methods
ok TestWeakSet.test_ne
ok TestWeakSet.test_new_or_init
ok TestWeakSet.test_or
ok TestWeakSet.test_pop
error TestWeakSet.test_remove: KeyError: <weakref at 0xX; to 'UserString' at 0xX>
ok TestWeakSet.test_repr
error TestWeakSet.test_sub: TypeError: 'generator' object is not an iterator
error TestWeakSet.test_sub_and_super: TypeError: 'generator' object is not an iterator
ok TestWeakSet.test_subclass_with_custom_hash
error TestWeakSet.test_symmetric_difference: TypeError: 'generator' object is not an iterator
error TestWeakSet.test_symmetric_difference_update: TypeError: 'generator' object is not an iterator
fail TestWeakSet.test_union: AssertionError: False != True
ok TestWeakSet.test_update
ok TestWeakSet.test_update_set
ok TestWeakSet.test_weak_destroy_and_mutate_while_iterating
ok TestWeakSet.test_weak_destroy_while_iterating
error TestWeakSet.test_xor: TypeError: 'generator' object is not an iterator
--- ran 46 ok 27 fail 5 error 14 skip 0 ---
