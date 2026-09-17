--- unittest ---
ok TestPkg.test_1
fail TestPkg.test_2: AssertionError: ['__doc__', 'self', 'sub', 't2'] != ['self', 'sub', 't2']
ok TestPkg.test_3
ok TestPkg.test_4
fail TestPkg.test_5: AssertionError: ['__doc__', 'foo', 'self', 'string', 't5'] != ['foo', 'self', 'string', 't5']
error TestPkg.test_6: ImportError: __all__ names nothing: spam
ok TestPkg.test_7
ok TestPkg.test_8
--- ran 8 ok 5 fail 2 error 1 skip 0 ---
