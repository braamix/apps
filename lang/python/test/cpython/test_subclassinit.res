...E......E....E.
======================================================================
ERROR: test_init_subclass_diamond (__main__.Test.test_init_subclass_diamond)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_subclassinit.py", line 96, in test_init_subclass_diamond
    class A(Left, Middle, Right, middle="middle"):
  File "/tmp/test_subclassinit.py", line 88, in __init_subclass__
    super().__init_subclass__(**kwargs)
TypeError: __init_subclass__() missing a required positional argument: 'cls'

======================================================================
ERROR: test_set_name_error (__main__.Test.test_set_name_error)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_subclassinit.py", line 141, in test_set_name_error
    notes = cm.exception.__notes__
AttributeError: 'ZeroDivisionError' object has no attribute '__notes__'

======================================================================
ERROR: test_set_name_wrong (__main__.Test.test_set_name_wrong)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_subclassinit.py", line 155, in test_set_name_wrong
    notes = cm.exception.__notes__
AttributeError: 'TypeError' object has no attribute '__notes__'

----------------------------------------------------------------------
Ran 17 tests in Ns

FAILED (errors=3)
