FFFFFF.EF.ssF......FFEF.sFFFFFF
======================================================================
ERROR: test_property_with_slots_and_doc_slot_docstring_present (__main__.PropertySubclassTests.test_property_with_slots_and_doc_slot_docstring_present)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 353, in test_property_with_slots_and_doc_slot_docstring_present
    self.assertEqual("what's up", p.__doc__)  # new in 3.12: This gets set.
AttributeError: 'slotted_prop' object has no attribute '__doc__'

======================================================================
ERROR: test_property_name (__main__.PropertyTests.test_property_name)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 229, in test_property_name
    self.assertEqual(A.quux.__name__, 'getter')
AttributeError: __name__ is not set

======================================================================
FAIL: test_docstring_copy (__main__.PropertySubclassTests.test_docstring_copy)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 402, in test_docstring_copy
    self.assertEqual(
AssertionError: 'This is a subclass of property' != 'spam wrapped in property subclass'
- This is a subclass of property
+ spam wrapped in property subclass


======================================================================
FAIL: test_docstring_copy2 (__main__.PropertySubclassTests.test_docstring_copy2)
Property tries to provide the best docstring it finds for its instances.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 445, in test_docstring_copy2
    self.assertEqual(p.__doc__, "doc 2")
AssertionError: None != 'doc 2'

======================================================================
FAIL: test_issue41287 (__main__.PropertySubclassTests.test_issue41287)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 370, in test_issue41287
    self.assertEqual(doc, "issue 41287 is fixed",
AssertionError: 'This is a subclass of property' != 'issue 41287 is fixed'
- This is a subclass of property
+ issue 41287 is fixed
 : Subclasses of `property` ignores `doc` constructor argument

======================================================================
FAIL: test_prefer_explicit_doc (__main__.PropertySubclassTests.test_prefer_explicit_doc)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 471, in test_prefer_explicit_doc
    self.assertEqual(PropertySub(doc="explicit doc").__doc__, "explicit doc")
AssertionError: 'This is a subclass of property' != 'explicit doc'
- This is a subclass of property
+ explicit doc


======================================================================
FAIL: test_property_new_getter_new_docstring (__main__.PropertySubclassTests.test_property_new_getter_new_docstring)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 546, in test_property_new_getter_new_docstring
    self.assertEqual(Foo.spam.__doc__, "a new docstring")
AssertionError: 'a docstring' != 'a new docstring'
- a docstring
+ a new docstring
?  ++++


======================================================================
FAIL: test_property_no_doc_on_getter (__main__.PropertySubclassTests.test_property_no_doc_on_getter)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 498, in test_property_no_doc_on_getter
    self.assertEqual(PropertySub(NoDoc()).__doc__, None)
AssertionError: 'This is a subclass of property' != None

======================================================================
FAIL: test_property_with_slots_docstring_silently_dropped (__main__.PropertySubclassTests.test_property_with_slots_docstring_silently_dropped)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 342, in test_property_with_slots_docstring_silently_dropped
    with self.assertRaises(AttributeError):
AssertionError: AttributeError not raised

======================================================================
FAIL: test_property___isabstractmethod__descriptor (__main__.PropertyTests.test_property___isabstractmethod__descriptor)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 146, in test_property___isabstractmethod__descriptor
    with self.assertRaises(ValueError):
AssertionError: ValueError not raised

======================================================================
FAIL: test_property_decorator_subclass_doc (__main__.PropertyTests.test_property_decorator_subclass_doc)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 107, in test_property_decorator_subclass_doc
    self.assertEqual(sub.__class__.spam.__doc__, "SubClass.getter")
AssertionError: 'BaseClass.getter' != 'SubClass.getter'
- BaseClass.getter
? ^^^^
+ SubClass.getter
? ^^^


======================================================================
FAIL: test_property_getter_doc_override (__main__.PropertyTests.test_property_getter_doc_override)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 126, in test_property_getter_doc_override
    self.assertEqual(newgettersub.__class__.spam.__doc__, "new docstring")
AssertionError: 'BaseClass.getter' != 'new docstring'
- BaseClass.getter
+ new docstring


======================================================================
FAIL: test_property_set_name_incorrect_args (__main__.PropertyTests.test_property_set_name_incorrect_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 261, in test_property_set_name_incorrect_args
    with self.assertRaisesRegex(
AssertionError: "^__set_name__\(\) takes 2 positional arguments but 0 were given$" does not match "__set_name__() takes exactly 2 arguments (0 given)"

======================================================================
FAIL: test_del_property (__main__.PropertyUnreachableAttributeNoName.test_del_property)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 581, in test_del_property
    with self.assertRaisesRegex(AttributeError, self._format_exc_msg("has no deleter")):
AssertionError: "^property of 'PropertyUnreachableAttributeNoName\.cls' object has no deleter$" does not match "can't delete attribute: foo"

======================================================================
FAIL: test_get_property (__main__.PropertyUnreachableAttributeNoName.test_get_property)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 573, in test_get_property
    with self.assertRaisesRegex(AttributeError, self._format_exc_msg("has no getter")):
AssertionError: "^property of 'PropertyUnreachableAttributeNoName\.cls' object has no getter$" does not match "unreadable attribute"

======================================================================
FAIL: test_set_property (__main__.PropertyUnreachableAttributeNoName.test_set_property)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 577, in test_set_property
    with self.assertRaisesRegex(AttributeError, self._format_exc_msg("has no setter")):
AssertionError: "^property of 'PropertyUnreachableAttributeNoName\.cls' object has no setter$" does not match "can't set attribute: foo"

======================================================================
FAIL: test_del_property (__main__.PropertyUnreachableAttributeWithName.test_del_property)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 581, in test_del_property
    with self.assertRaisesRegex(AttributeError, self._format_exc_msg("has no deleter")):
AssertionError: "^property 'foo' of 'PropertyUnreachableAttributeWithName\.cls' object has no deleter$" does not match "can't delete attribute: foo"

======================================================================
FAIL: test_get_property (__main__.PropertyUnreachableAttributeWithName.test_get_property)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 573, in test_get_property
    with self.assertRaisesRegex(AttributeError, self._format_exc_msg("has no getter")):
AssertionError: "^property 'foo' of 'PropertyUnreachableAttributeWithName\.cls' object has no getter$" does not match "unreadable attribute"

======================================================================
FAIL: test_set_property (__main__.PropertyUnreachableAttributeWithName.test_set_property)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_property.py", line 577, in test_set_property
    with self.assertRaisesRegex(AttributeError, self._format_exc_msg("has no setter")):
AssertionError: "^property 'foo' of 'PropertyUnreachableAttributeWithName\.cls' object has no setter$" does not match "can't set attribute: foo"

----------------------------------------------------------------------
Ran 31 tests in Ns

FAILED (failures=17, errors=2, skipped=3)
