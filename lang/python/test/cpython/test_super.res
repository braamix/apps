...sF.....F.......s..s..E.....F.F.....sE
======================================================================
ERROR: test_shadowed_global (__main__.TestSuper.test_shadowed_global)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_super.py", line 356, in test_shadowed_global
    with import_helper.ready_to_import(name="shadowed_super", source=source):
AttributeError: module 'test.support.import_helper' has no attribute 'ready_to_import'

======================================================================
ERROR: test_various___class___pathologies (__main__.TestSuper.test_various___class___pathologies)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_super.py", line 102, in test_various___class___pathologies
    class X:
  File "/tmp/test_super.py", line 103, in X
    x = __class__
NameError: name '__class__' is not defined

======================================================================
FAIL: test___class___mro (__main__.TestSuper.test___class___mro)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_super.py", line 205, in test___class___mro
    self.assertIs(test_class, A)
AssertionError: None is not <class '__main__.TestSuper.test___class___mro.<locals>.A'>

======================================================================
FAIL: test___classcell___wrong_cell (__main__.TestSuper.test___classcell___wrong_cell)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_super.py", line 284, in test___classcell___wrong_cell
    with self.assertRaises(TypeError):
AssertionError: TypeError not raised

======================================================================
FAIL: test_super_argcount (__main__.TestSuper.test_super_argcount)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_super.py", line 338, in test_super_argcount
    with self.assertRaisesRegex(TypeError, "expected at most"):
AssertionError: "expected at most" does not match "super() takes exactly 2 arguments (3 given)"

======================================================================
FAIL: test_super_in_class_methods_working (__main__.TestSuper.test_super_in_class_methods_working)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_super.py", line 78, in test_super_in_class_methods_working
    self.assertEqual(d.cm(), (d, (D, (D, (D, 'A'), 'B'), 'C'), 'D'))
AssertionError: Tuples differ: (<D o[33 chars]in__.C'>, (<class '__main__.A'>, 'A'), 'C'), 'D') != (<D o[33 chars]in__.D'>, (<class '__main__.D'>, (<class '__ma[27 chars] 'D')

First differing element 1:
(<class '__main__.C'>, (<class '__main__.A'>, 'A'), 'C')
(<class '__main__.D'>, (<class '__main__.D'>, (<class '__ma[21 chars] 'C')

  (<D object at 0xX>,
+  (<class '__main__.D'>,
-  (<class '__main__.C'>, (<class '__main__.A'>, 'A'), 'C'),
?                    ^                      ^           ^

+   (<class '__main__.D'>, (<class '__main__.D'>, 'A'), 'B'),
? +                   ^                      ^           ^

+   'C'),
   'D')

----------------------------------------------------------------------
Ran 40 tests in Ns

FAILED (failures=4, errors=2, skipped=4)
