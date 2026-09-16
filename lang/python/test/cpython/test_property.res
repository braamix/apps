--- unittest ---
fail PropertySubclassTests.test_docstring_copy: AssertionError: 'This is a subclass of property' != 'spam wrapped in property subclass'
fail PropertySubclassTests.test_docstring_copy2: AssertionError: None != 'doc 2'
fail PropertySubclassTests.test_issue41287: AssertionError: 'This is a subclass of property' != 'issue 41287 is fixed' : Subclasses of `property` ignores `doc` constructor argument
fail PropertySubclassTests.test_prefer_explicit_doc: AssertionError: 'This is a subclass of property' != 'explicit doc'
fail PropertySubclassTests.test_property_new_getter_new_docstring: AssertionError: 'a docstring' != 'a new docstring'
fail PropertySubclassTests.test_property_no_doc_on_getter: AssertionError: 'This is a subclass of property' != None
ok PropertySubclassTests.test_property_setter_copies_getter_docstring
error PropertySubclassTests.test_property_with_slots_and_doc_slot_docstring_present: AttributeError: object has no attribute: __doc__
fail PropertySubclassTests.test_property_with_slots_docstring_silently_dropped: AssertionError: AttributeError not raised
ok PropertySubclassTests.test_property_with_slots_no_docstring
skip PropertySubclassTests.test_slots_docstring_copy_exception: test requires docstrings
skip PropertyTests.test_gh_115618: the collector does not count references
fail PropertyTests.test_property___isabstractmethod__descriptor: AssertionError: ValueError not raised
ok PropertyTests.test_property_builtin_doc_writable
ok PropertyTests.test_property_decorator_baseclass
ok PropertyTests.test_property_decorator_baseclass_doc
ok PropertyTests.test_property_decorator_doc
ok PropertyTests.test_property_decorator_doc_writable
ok PropertyTests.test_property_decorator_subclass
fail PropertyTests.test_property_decorator_subclass_doc: AssertionError: 'BaseClass.getter' != 'SubClass.getter'
fail PropertyTests.test_property_getter_doc_override: AssertionError: 'BaseClass.getter' != 'new docstring'
error PropertyTests.test_property_name: AttributeError: __name__ is not set
fail PropertyTests.test_property_set_name_incorrect_args: AssertionError: '^__set_name__\\(\\) takes 2 positional arguments but 0 were given$' does not match '__set_name__() takes exactly 2 arguments (0 given)'
ok PropertyTests.test_property_setname_on_property_subclass
skip PropertyTests.test_refleaks_in___init__: the collector does not count references
fail PropertyUnreachableAttributeNoName.test_del_property: AssertionError: "^property of 'PropertyUnreachableAttributeNoName\\.cls' object has no deleter$" does not match "can't delete attribute: foo"
fail PropertyUnreachableAttributeNoName.test_get_property: AssertionError: "^property of 'PropertyUnreachableAttributeNoName\\.cls' object has no getter$" does not match 'unreadable attribute'
fail PropertyUnreachableAttributeNoName.test_set_property: AssertionError: "^property of 'PropertyUnreachableAttributeNoName\\.cls' object has no setter$" does not match "can't set attribute: foo"
fail PropertyUnreachableAttributeWithName.test_del_property: AssertionError: "^property 'foo' of 'PropertyUnreachableAttributeWithName\\.cls' object has no deleter$" does not match "can't delete attribute: foo"
fail PropertyUnreachableAttributeWithName.test_get_property: AssertionError: "^property 'foo' of 'PropertyUnreachableAttributeWithName\\.cls' object has no getter$" does not match 'unreadable attribute'
fail PropertyUnreachableAttributeWithName.test_set_property: AssertionError: "^property 'foo' of 'PropertyUnreachableAttributeWithName\\.cls' object has no setter$" does not match "can't set attribute: foo"
--- ran 31 ok 9 fail 17 error 2 skip 3 ---
