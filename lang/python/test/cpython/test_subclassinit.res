--- unittest ---
ok Test.test_errors
ok Test.test_errors_changed_pep487
ok Test.test_init_subclass
error Test.test_init_subclass_diamond: TypeError: __init_subclass__() missing a required positional argument: 'cls'
ok Test.test_init_subclass_dict
ok Test.test_init_subclass_error
ok Test.test_init_subclass_kwargs
ok Test.test_init_subclass_skipped
ok Test.test_init_subclass_wrong
ok Test.test_set_name
error Test.test_set_name_error: AttributeError: 'ZeroDivisionError' object has no attribute '__notes__'
ok Test.test_set_name_init_subclass
ok Test.test_set_name_lookup
ok Test.test_set_name_metaclass
ok Test.test_set_name_modifying_dict
error Test.test_set_name_wrong: AttributeError: 'TypeError' object has no attribute '__notes__'
ok Test.test_type
--- ran 17 ok 14 fail 0 error 3 skip 0 ---
