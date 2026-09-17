--- unittest ---
ok DictTest.test_dicts
fail ListTest.test_badentry: AssertionError: Exc not raised
ok ListTest.test_coverage
error ListTest.test_goodentry: TypeError: '<' not supported between instances of 'Good' and 'Good'
ok MiscTest.test_exception_message
ok MiscTest.test_misbehavin
fail MiscTest.test_not: AssertionError: Exc not raised
fail MiscTest.test_recursion: AssertionError: RecursionError not raised
ok NumberTest.test_basic
ok NumberTest.test_values
error VectorTest.test_mixed: TypeError: object of this type has no len(): bool
--- ran 11 ok 6 fail 3 error 2 skip 0 ---
