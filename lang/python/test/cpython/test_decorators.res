.....E.........E
======================================================================
ERROR: test_classmethod (__main__.TestDecorators.test_classmethod)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_decorators.py", line 103, in test_classmethod
    wrapper = self.check_wrapper_attrs(classmethod, '<classmethod({!r})>')
  File "/tmp/test_decorators.py", line 90, in check_wrapper_attrs
    self.assertIs(getattr(wrapper, attr),
AttributeError: 'classmethod' object has no attribute '__annotations__'

======================================================================
ERROR: test_staticmethod (__main__.TestDecorators.test_staticmethod)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_decorators.py", line 97, in test_staticmethod
    wrapper = self.check_wrapper_attrs(staticmethod, '<staticmethod({!r})>')
  File "/tmp/test_decorators.py", line 90, in check_wrapper_attrs
    self.assertIs(getattr(wrapper, attr),
AttributeError: 'staticmethod' object has no attribute '__annotations__'

----------------------------------------------------------------------
Ran 16 tests in Ns

FAILED (errors=2)
