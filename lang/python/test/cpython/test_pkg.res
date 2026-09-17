--- unittest ---
ok TestPkg.test_1
fail TestPkg.test_2: AssertionError: ['__doc__', 'self', 'sub', 't2'] != ['self', 'sub', 't2']
ok TestPkg.test_3
ok TestPkg.test_4
fail TestPkg.test_5: AssertionError: ['__doc__', 'foo', 'self', 'string', 't5'] != ['foo', 'self', 'string', 't5']
fail TestPkg.test_6: AssertionError: ['__all__', '__doc__', '__file__', '__name__', '__path__'] != ['__all__', '__doc__', '__file__', '__loader__', '__name__', '__package__', '__path__', '__spec__']
fail TestPkg.test_7: AssertionError: ['__doc__', '__file__', '__name__', '__path__'] != ['__doc__', '__file__', '__loader__', '__name__', '__package__', '__path__', '__spec__']
ok TestPkg.test_8
--- ran 8 ok 4 fail 4 error 0 skip 0 ---
