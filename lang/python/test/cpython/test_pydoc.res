.E..FF......ssssE.......FssE..Es.E.sF....sEs.FFFFFFEEEEEEs..ss.ss...FssFFFs..s.ssss..sss.sFss.sFFFFFFFFFFFFss.ssEFFssFFFs.....F....
======================================================================
ERROR: test_allmethods (__main__.PydocDocTest.test_allmethods)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1089, in test_allmethods
    del expected['__class__']
KeyError: '__class__'

======================================================================
ERROR: test_html_doc_undecodable_path (__main__.PydocDocTest.test_html_doc_undecodable_path)
----------------------------------------------------------------------
AttributeError: module 'test.support.import_helper' has no attribute 'DirsOnSysPath'

======================================================================
ERROR: test_mixed_case_module_names_are_lower_cased (__main__.PydocDocTest.test_mixed_case_module_names_are_lower_cased)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 470, in test_mixed_case_module_names_are_lower_cased
    self.assertIn('xml.etree.elementtree', doc_link)
  File "/pkg/store/python-0/lib/unittest/case.py", line 1213, in assertIn
    if member not in container:
TypeError: argument of type 'NoneType' is not a container or iterable

======================================================================
ERROR: test_non_str_name (__main__.PydocDocTest.test_non_str_name)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 519, in test_non_str_name
    adoc = pydoc.render_doc(A())
  File "/pkg/store/python-0/lib/pydoc.py", line 1748, in render_doc
    desc += ' in module ' + module.__name__
TypeError: unsupported operand type(s) for +: 'int' and 'str'

======================================================================
ERROR: test_online_docs_link (__main__.PydocDocTest.test_online_docs_link)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 478, in test_online_docs_link
    import encodings.idna
ModuleNotFoundError: No module named 'encodings.idna'. Did you mean: 'encodings.ascii'?

======================================================================
ERROR: test_synopsis_sourceless_empty_doc (__main__.PydocDocTest.test_synopsis_sourceless_empty_doc)
----------------------------------------------------------------------
ValueError: unmarshallable object

======================================================================
ERROR: test_apropos_empty_doc (__main__.PydocImportTest.test_apropos_empty_doc)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1362, in setUp
    importlib.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/__init__.py", line 68, in invalidate_caches
    finder.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/_bootstrap_external.py", line 1202, in invalidate_caches
    from importlib.metadata import MetadataPathFinder
ModuleNotFoundError: No module named 'importlib.metadata'

======================================================================
ERROR: test_apropos_with_bad_package (__main__.PydocImportTest.test_apropos_with_bad_package)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1362, in setUp
    importlib.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/__init__.py", line 68, in invalidate_caches
    finder.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/_bootstrap_external.py", line 1202, in invalidate_caches
    from importlib.metadata import MetadataPathFinder
ModuleNotFoundError: No module named 'importlib.metadata'

======================================================================
ERROR: test_apropos_with_unreadable_dir (__main__.PydocImportTest.test_apropos_with_unreadable_dir)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1362, in setUp
    importlib.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/__init__.py", line 68, in invalidate_caches
    finder.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/_bootstrap_external.py", line 1202, in invalidate_caches
    from importlib.metadata import MetadataPathFinder
ModuleNotFoundError: No module named 'importlib.metadata'

======================================================================
ERROR: test_badimport (__main__.PydocImportTest.test_badimport)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1362, in setUp
    importlib.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/__init__.py", line 68, in invalidate_caches
    finder.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/_bootstrap_external.py", line 1202, in invalidate_caches
    from importlib.metadata import MetadataPathFinder
ModuleNotFoundError: No module named 'importlib.metadata'

======================================================================
ERROR: test_importfile (__main__.PydocImportTest.test_importfile)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1362, in setUp
    importlib.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/__init__.py", line 68, in invalidate_caches
    finder.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/_bootstrap_external.py", line 1202, in invalidate_caches
    from importlib.metadata import MetadataPathFinder
ModuleNotFoundError: No module named 'importlib.metadata'

======================================================================
ERROR: test_url_search_package_error (__main__.PydocImportTest.test_url_search_package_error)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1362, in setUp
    importlib.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/__init__.py", line 68, in invalidate_caches
    finder.invalidate_caches()
  File "/pkg/store/python-0/lib/importlib/_bootstrap_external.py", line 1202, in invalidate_caches
    from importlib.metadata import MetadataPathFinder
ModuleNotFoundError: No module named 'importlib.metadata'

======================================================================
ERROR: test_typing_pydoc (__main__.TestDescriptions.test_typing_pydoc)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1542, in test_typing_pydoc
    class C(typing.Generic[T], typing.Mapping[int, str]): ...
  File "/pkg/store/python-0/lib/abc.py", line 127, in __new__
    cls = super().__new__(mcls, name, bases, namespace, **kwargs)
TypeError: cannot create a consistent method resolution order

======================================================================
FAIL: test_builtin_with_child (__main__.PydocDocTest.test_builtin_with_child)
Tests help on builtin object which have only child classes.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 619, in test_builtin_with_child
    self.assertIn(snip, text)
AssertionError: ' |  Built-in subclasses:\n |      FloatingPointError\n |      OverflowError\n |      ZeroDivisionError' not found in 'class A\x08Ar\x08ri\x08it\x08th\x08hm\x08me\x08et\x08ti\x08ic\x08cE\x08Er\x08rr\x08ro\x08or\x08r(Exception)\n |  Method resolution order:\n |      ArithmeticError\n |      Exception\n |      BaseException\n |      object\n |\n |  Static methods inherited from BaseException:\n |\n |  _\x08__\x08_i\x08in\x08ni\x08it\x08t_\x08__\x08_(...)\n |\n |  _\x08__\x08_r\x08re\x08ed\x08du\x08uc\x08ce\x08e_\x08__\x08_(...)\n |\n |  _\x08__\x08_s\x08se\x08et\x08ts\x08st\x08ta\x08at\x08te\x08e_\x08__\x08_(...)\n |\n |  a\x08ad\x08dd\x08d_\x08_n\x08no\x08ot\x08te\x08e(...)\n |\n |  w\x08wi\x08it\x08th\x08h_\x08_t\x08tr\x08ra\x08ac\x08ce\x08eb\x08ba\x08ac\x08ck\x08k(...)\n'

======================================================================
FAIL: test_builtin_with_grandchild (__main__.PydocDocTest.test_builtin_with_grandchild)
Tests help on builtin classes which have grandchild classes.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 649, in test_builtin_with_grandchild
    self.assertIn(snip, text)
AssertionError: ' |  Built-in subclasses:\n |      ArithmeticError\n |      AssertionError\n |      AttributeError' not found in 'class E\x08Ex\x08xc\x08ce\x08ep\x08pt\x08ti\x08io\x08on\x08n(BaseException)\n |  Method resolution order:\n |      Exception\n |      BaseException\n |      object\n |\n |  Static methods inherited from BaseException:\n |\n |  _\x08__\x08_i\x08in\x08ni\x08it\x08t_\x08__\x08_(...)\n |\n |  _\x08__\x08_r\x08re\x08ed\x08du\x08uc\x08ce\x08e_\x08__\x08_(...)\n |\n |  _\x08__\x08_s\x08se\x08et\x08ts\x08st\x08ta\x08at\x08te\x08e_\x08__\x08_(...)\n |\n |  a\x08ad\x08dd\x08d_\x08_n\x08no\x08ot\x08te\x08e(...)\n |\n |  w\x08wi\x08it\x08th\x08h_\x08_t\x08tr\x08ra\x08ac\x08ce\x08eb\x08ba\x08ac\x08ck\x08k(...)\n'

======================================================================
FAIL: test_long_signatures (__main__.PydocDocTest.test_long_signatures)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1202, in test_long_signatures
    self.assertEqual(doc, '''Python Library Documentation: class A in module %s
AssertionError: 'Python Library Documentation: class A in[92 chars].)\n' != "Python Library Documentation: class A in[618 chars]__\n"
  Python Library Documentation: class A in module __main__
  
  class A(builtins.object)
+  |  A(
+  |      arg1: Callable[[int, int, int], str],
+  |      arg2: Literal['some value', 'other value'],
+  |      arg3: Annotated[int, 'some docs about this type']
+  |  ) -> None
+  |
   |  Methods defined here:
   |
-  |  __init__(...)
?              ----
+  |  __init__(
+  |      self,
+  |      arg1: Callable[[int, int, int], str],
+  |      arg2: Literal['some value', 'other value'],
+  |      arg3: Annotated[int, 'some docs about this type']
+  |  ) -> None
+  |
+  |  ----------------------------------------------------------------------
+  |  Data descriptors defined here:
+  |
+  |  __dict__
+  |
+  |  __weakref__


======================================================================
FAIL: test_slotted_dataclass_with_field_docs (__main__.PydocDocTest.test_slotted_dataclass_with_field_docs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 465, in test_slotted_dataclass_with_field_docs
    self.assertIn('Docstring for x', doc)
AssertionError: 'Docstring for x' not found in "Python Library Documentation: class My in module __main__\n\nclass M\x08My\x08y(builtins.object)\n |  My(x: int)\n |\n |  Methods defined here:\n |\n |  _\x08__\x08_e\x08eq\x08q_\x08__\x08_(self, other)\n |\n |  _\x08__\x08_i\x08in\x08ni\x08it\x08t_\x08__\x08_(...)\n |\n |  _\x08__\x08_r\x08re\x08ep\x08pl\x08la\x08ac\x08ce\x08e_\x08__\x08_ = _replace(self, /, **changes) from dataclasses\n |\n |  _\x08__\x08_r\x08re\x08ep\x08pr\x08r_\x08__\x08_(self) from reprlib.PydocDocTest.test_slotted_dataclass_with_field_docs.<locals>.My\n |\n |  ----------------------------------------------------------------------\n |  Data descriptors defined here:\n |\n |  x\x08x\n |\n |  ----------------------------------------------------------------------\n |  Data and other attributes defined here:\n |\n |  _\x08__\x08_d\x08da\x08at\x08ta\x08ac\x08cl\x08la\x08as\x08ss\x08s_\x08_f\x08fi\x08ie\x08el\x08ld\x08ds\x08s_\x08__\x08_ = {'x': Field(name='x',type=<class 'int'>,defaul...\n |\n |  _\x08__\x08_d\x08da\x08at\x08ta\x08ac\x08cl\x08la\x08as\x08ss\x08s_\x08_p\x08pa\x08ar\x08ra\x08am\x08ms\x08s_\x08__\x08_ = _DataclassParams(init=True,repr=True,eq=True,o...\n |\n |  _\x08__\x08_h\x08ha\x08as\x08sh\x08h_\x08__\x08_ = None\n |\n |  _\x08__\x08_m\x08ma\x08at\x08tc\x08ch\x08h_\x08_a\x08ar\x08rg\x08gs\x08s_\x08__\x08_ = ('x',)\n"

======================================================================
FAIL: test_html_doc_inherited_routines_in_class (__main__.PydocFodderTest.test_html_doc_inherited_routines_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 2068, in test_html_doc_inherited_routines_in_class
    self.test_html_doc_routines_in_class(pydocfodder.D)
  File "/tmp/test_pydoc.py", line 2049, in test_html_doc_routines_in_class
    self.assertIn('count(self, object, /) from builtins.list', lines)
AssertionError: 'count(self, object, /) from builtins.list' not found in ['Methods inherited from B:', 'ABC_method(self)', 'Method defined in A, B and C.', '', 'AB_method(self)', 'Method defined in A and B.', '', 'A_method_alias = A_method(self)', '', 'A_staticmethod(x, y) from test.test_pydoc.pydocfodder.A', 'A static method defined in A.', '', 'A_staticmethod_alias = A_staticmethod(x, y)', '', 'BC_method(self)', 'Method defined in B and C.', '', 'B_method(self)', 'Method defined in B.', '', 'B_method_alias = B_method(self)', '', 'global_func(x, y) from test.test_pydoc.pydocfodder', 'Module global function', '', 'global_func2_alias = global_func2(x, y) from test.test_pydoc.pydocfodder', 'Module global function 2', '', 'global_func_alias = global_func(x, y)', '', '']

======================================================================
FAIL: test_html_doc_routines_in_class (__main__.PydocFodderTest.test_html_doc_routines_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 2049, in test_html_doc_routines_in_class
    self.assertIn('count(self, object, /) from builtins.list', lines)
AssertionError: 'count(self, object, /) from builtins.list' not found in ['Methods defined here:', 'ABCD_method(self)', 'Method defined in A, B, C and D.', '', 'ABC_method(self)', 'Method defined in A, B and C.', '', 'ABD_method(self)', 'Method defined in A, B and D.', '', 'AB_method(self)', 'Method defined in A and B.', '', 'A_method_alias = A_method(self)', '', 'A_staticmethod(x, y) from test.test_pydoc.pydocfodder.A', 'A static method defined in A.', '', 'A_staticmethod_alias = A_staticmethod(x, y)', '', 'BCD_method(self)', 'Method defined in B, C and D.', '', 'BC_method(self)', 'Method defined in B and C.', '', 'BD_method(self)', 'Method defined in B and D.', '', 'B_method(self)', 'Method defined in B.', '', 'B_method_alias = B_method(self)', '', 'global_func(x, y) from test.test_pydoc.pydocfodder', 'Module global function', '', 'global_func2_alias = global_func2(x, y) from test.test_pydoc.pydocfodder', 'Module global function 2', '', 'global_func_alias = global_func(x, y)', '']

======================================================================
FAIL: test_html_doc_routines_in_module (__main__.PydocFodderTest.test_html_doc_routines_in_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 2150, in test_html_doc_routines_in_module
    self.assertIn(' count(self, object, /) unbound builtins.list method', lines)
AssertionError: ' count(self, object, /) unbound builtins.list method' not found in ['Functions', 'A_classmethod(x) class method of A', 'A class method defined in A.', ' A_classmethod2 = A_classmethod(x) class method of A', 'A class method defined in A.', ' A_classmethod3 = A_classmethod(x) class method of B', 'A class method defined in A.', ' A_method() method of A instance', 'Method defined in A.', ' A_method2 = A_method() method of A instance', 'Method defined in A.', ' A_method3 = A_method() method of B instance', 'Method defined in A.', ' A_staticmethod(x, y)', 'A static method defined in A.', ' A_staticmethod_alias = A_staticmethod(x, y)', 'A static method defined in A.', ' A_staticmethod_ref = A_staticmethod(x, y)', 'A static method defined in A.', ' A_staticmethod_ref2 = A_staticmethod(y) method of B instance', 'A static method defined in A.', ' B_method(self)', 'Method defined in B.', ' B_method2 = B_method(self)', 'Method defined in B.', ' __repr__ = object(...)', ' count(...)', ' dict_get = get(...) method of builtins.dict instance', ' get(...) method of builtins.dict instance', ' global_func(x, y)', 'Module global function', ' global_func2(x, y)', 'Module global function 2', ' global_func_alias = global_func(x, y)', 'Module global function', ' list_count = count(...)', ' object_repr = object(...)', ' sin(...)', '', '']

======================================================================
FAIL: test_text_doc_inherited_routines_in_class (__main__.PydocFodderTest.test_text_doc_inherited_routines_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 2065, in test_text_doc_inherited_routines_in_class
    self.test_text_doc_routines_in_class(pydocfodder.D)
  File "/tmp/test_pydoc.py", line 2007, in test_text_doc_routines_in_class
    self.assertIn(' |  count(self, object, /) from builtins.list', lines)
AssertionError: ' |  count(self, object, /) from builtins.list' not found in [' |  Methods inherited from B:', ' |', ' |  ABC_method(self)', ' |      Method defined in A, B and C.', ' |', ' |  AB_method(self)', ' |      Method defined in A and B.', ' |', ' |  A_method_alias = A_method(self)', ' |', ' |  A_staticmethod(x, y) from test.test_pydoc.pydocfodder.A', ' |      A static method defined in A.', ' |', ' |  A_staticmethod_alias = A_staticmethod(x, y)', ' |', ' |  BC_method(self)', ' |      Method defined in B and C.', ' |', ' |  B_method(self)', ' |      Method defined in B.', ' |', ' |  B_method_alias = B_method(self)', ' |', ' |  global_func(x, y) from test.test_pydoc.pydocfodder', ' |      Module global function', ' |', ' |  global_func2_alias = global_func2(x, y) from test.test_pydoc.pydocfodder', ' |      Module global function 2', ' |', ' |  global_func_alias = global_func(x, y)', ' |']

======================================================================
FAIL: test_text_doc_routines_in_class (__main__.PydocFodderTest.test_text_doc_routines_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 2007, in test_text_doc_routines_in_class
    self.assertIn(' |  count(self, object, /) from builtins.list', lines)
AssertionError: ' |  count(self, object, /) from builtins.list' not found in [' |  Methods defined here:', ' |', ' |  ABCD_method(self)', ' |      Method defined in A, B, C and D.', ' |', ' |  ABC_method(self)', ' |      Method defined in A, B and C.', ' |', ' |  ABD_method(self)', ' |      Method defined in A, B and D.', ' |', ' |  AB_method(self)', ' |      Method defined in A and B.', ' |', ' |  A_method_alias = A_method(self)', ' |', ' |  A_staticmethod(x, y) from test.test_pydoc.pydocfodder.A', ' |      A static method defined in A.', ' |', ' |  A_staticmethod_alias = A_staticmethod(x, y)', ' |', ' |  BCD_method(self)', ' |      Method defined in B, C and D.', ' |', ' |  BC_method(self)', ' |      Method defined in B and C.', ' |', ' |  BD_method(self)', ' |      Method defined in B and D.', ' |', ' |  B_method(self)', ' |      Method defined in B.', ' |', ' |  B_method_alias = B_method(self)', ' |', ' |  global_func(x, y) from test.test_pydoc.pydocfodder', ' |      Module global function', ' |', ' |  global_func2_alias = global_func2(x, y) from test.test_pydoc.pydocfodder', ' |      Module global function 2', ' |', ' |  global_func_alias = global_func(x, y)', ' |']

======================================================================
FAIL: test_text_doc_routines_in_module (__main__.PydocFodderTest.test_text_doc_routines_in_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 2105, in test_text_doc_routines_in_module
    self.assertIn('    count(self, object, /) unbound builtins.list method', lines)
AssertionError: '    count(self, object, /) unbound builtins.list method' not found in ['FUNCTIONS', '    A_classmethod(x) class method of A', '        A class method defined in A.', '', '    A_classmethod2 = A_classmethod(x) class method of A', '        A class method defined in A.', '', '    A_classmethod3 = A_classmethod(x) class method of B', '        A class method defined in A.', '', '    A_method() method of A instance', '        Method defined in A.', '', '    A_method2 = A_method() method of A instance', '        Method defined in A.', '', '    A_method3 = A_method() method of B instance', '        Method defined in A.', '', '    A_staticmethod(x, y)', '        A static method defined in A.', '', '    A_staticmethod_alias = A_staticmethod(x, y)', '        A static method defined in A.', '', '    A_staticmethod_ref = A_staticmethod(x, y)', '        A static method defined in A.', '', '    A_staticmethod_ref2 = A_staticmethod(y) method of B instance', '        A static method defined in A.', '', '    B_method(self)', '        Method defined in B.', '', '    B_method2 = B_method(self)', '        Method defined in B.', '', '    __repr__ = object(...)', '', '    count(...)', '', '    dict_get = get(...) method of builtins.dict instance', '', '    get(...) method of builtins.dict instance', '', '    global_func(x, y)', '        Module global function', '', '    global_func2(x, y)', '        Module global function 2', '', '    global_func_alias = global_func(x, y)', '        Module global function', '', '    list_count = count(...)', '', '    object_repr = object(...)', '', '    sin(...)', '']

======================================================================
FAIL: test_bound_builtin_classmethod_o (__main__.TestDescriptions.test_bound_builtin_classmethod_o)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1674, in test_bound_builtin_classmethod_o
    self.assertEqual(self._get_summary_line(dict.__class_getitem__),
AssertionError: '__class_getitem__(...)' != '__class_getitem__(object, /) class method of builtins.dict'
- __class_getitem__(...)
+ __class_getitem__(object, /) class method of builtins.dict


======================================================================
FAIL: test_bound_builtin_method_coexist_o (__main__.TestDescriptions.test_bound_builtin_method_coexist_o)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1658, in test_bound_builtin_method_coexist_o
    self.assertEqual(self._get_summary_line(set().__contains__),
AssertionError: '__contains__(...) method of builtins.set instance' != '__contains__(object, /) method of builtins.set instance'
- __contains__(...) method of builtins.set instance
?              ^^^
+ __contains__(object, /) method of builtins.set instance
?              ^^^^^^^^^


======================================================================
FAIL: test_bound_builtin_method_noargs (__main__.TestDescriptions.test_bound_builtin_method_noargs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1642, in test_bound_builtin_method_noargs
    self.assertEqual(self._get_summary_line(''.lower),
AssertionError: 'lower(...) method of builtins.str instance' != 'lower() method of builtins.str instance'
- lower(...) method of builtins.str instance
?       ---
+ lower() method of builtins.str instance


======================================================================
FAIL: test_bound_builtin_method_o (__main__.TestDescriptions.test_bound_builtin_method_o)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1650, in test_bound_builtin_method_o
    self.assertEqual(self._get_summary_line(set().add),
AssertionError: 'add(...) method of builtins.set instance' != 'add(object, /) method of builtins.set instance'
- add(...) method of builtins.set instance
?     ^^^
+ add(object, /) method of builtins.set instance
?     ^^^^^^^^^


======================================================================
FAIL: test_module_level_callable_noargs (__main__.TestDescriptions.test_module_level_callable_noargs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1624, in test_module_level_callable_noargs
    self.assertEqual(self._get_summary_line(time.time),
AssertionError: 'time(...)' != 'time()'
- time(...)
?      ---
+ time()


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [($slf)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth() method of __main__.C instance' != 'meth() method of test.test_pydoc.test_pydoc.C instance'
- meth() method of __main__.C instance
+ meth() method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [($slf, /)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth() method of __main__.C instance' != 'meth() method of test.test_pydoc.test_pydoc.C instance'
- meth() method of __main__.C instance
+ meth() method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [($slf, /, arg)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg) method of __main__.C instance' != 'meth(arg) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg) method of __main__.C instance
+ meth(arg) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [($slf, /, arg=<x>)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg=<x>) method of __main__.C instance' != 'meth(arg=<x>) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg=<x>) method of __main__.C instance
+ meth(arg=<x>) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [($slf, arg, /)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg, /) method of __main__.C instance' != 'meth(arg, /) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg, /) method of __main__.C instance
+ meth(arg, /) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [($slf, arg=<x>, /)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg=<x>, /) method of __main__.C instance' != 'meth(arg=<x>, /) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg=<x>, /) method of __main__.C instance
+ meth(arg=<x>, /) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [(/, slf, arg)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1764, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C.meth),
AssertionError: 'meth(slf, arg)' != 'meth(/, slf, arg)'
- meth(slf, arg)
+ meth(/, slf, arg)
?      +++


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [(/, slf, arg=<x>)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(/, slf, arg=<x>) method of __main__.C instance' != 'meth(/, slf, arg=<x>) method of test.test_pydoc.test_pydoc.C instance'
- meth(/, slf, arg=<x>) method of __main__.C instance
?                                   ^^^^^^
+ meth(/, slf, arg=<x>) method of test.test_pydoc.test_pydoc.C instance
?                                 +++++++++ ++++++++++ ^^^^^


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [(slf, /, arg)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg) method of __main__.C instance' != 'meth(arg) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg) method of __main__.C instance
+ meth(arg) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [(slf, /, arg=<x>)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg=<x>) method of __main__.C instance' != 'meth(arg=<x>) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg=<x>) method of __main__.C instance
+ meth(arg=<x>) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [(slf, arg, /)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg, /) method of __main__.C instance' != 'meth(arg, /) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg, /) method of __main__.C instance
+ meth(arg, /) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_overridden_text_signature (__main__.TestDescriptions.test_overridden_text_signature) [(slf, arg=<x>, /)]
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1766, in test_overridden_text_signature
    self.assertEqual(self._get_summary_line(C().meth),
AssertionError: 'meth(arg=<x>, /) method of __main__.C instance' != 'meth(arg=<x>, /) method of test.test_pydoc.test_pydoc.C instance'
- meth(arg=<x>, /) method of __main__.C instance
+ meth(arg=<x>, /) method of test.test_pydoc.test_pydoc.C instance


======================================================================
FAIL: test_unbound_builtin_classmethod_noargs (__main__.TestDescriptions.test_unbound_builtin_classmethod_noargs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1662, in test_unbound_builtin_classmethod_noargs
    self.assertEqual(self._get_summary_line(datetime.datetime.__dict__['utcnow']),
AssertionError: 'utcnow(...)' != 'utcnow(type, /) unbound datetime.datetime method'
- utcnow(...)
+ utcnow(type, /) unbound datetime.datetime method


======================================================================
FAIL: test_unbound_builtin_classmethod_o (__main__.TestDescriptions.test_unbound_builtin_classmethod_o)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1670, in test_unbound_builtin_classmethod_o
    self.assertEqual(self._get_summary_line(dict.__dict__['__class_getitem__']),
AssertionError: '__class_getitem__(...)' != '__class_getitem__(type, object, /) unbound builtins.dict method'
- __class_getitem__(...)
+ __class_getitem__(type, object, /) unbound builtins.dict method


======================================================================
FAIL: test_unbound_builtin_method_coexist_o (__main__.TestDescriptions.test_unbound_builtin_method_coexist_o)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1654, in test_unbound_builtin_method_coexist_o
    self.assertEqual(self._get_summary_line(set.__contains__),
AssertionError: '__contains__(...)' != '__contains__(self, object, /) unbound builtins.set method'
- __contains__(...)
+ __contains__(self, object, /) unbound builtins.set method


======================================================================
FAIL: test_unbound_builtin_method_noargs (__main__.TestDescriptions.test_unbound_builtin_method_noargs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1638, in test_unbound_builtin_method_noargs
    self.assertEqual(self._get_summary_line(str.lower),
AssertionError: 'lower(...)' != 'lower(self, /) unbound builtins.str method'
- lower(...)
+ lower(self, /) unbound builtins.str method


======================================================================
FAIL: test_unbound_builtin_method_o (__main__.TestDescriptions.test_unbound_builtin_method_o)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 1646, in test_unbound_builtin_method_o
    self.assertEqual(self._get_summary_line(set.add),
AssertionError: 'add(...)' != 'add(self, object, /) unbound builtins.set method'
- add(...)
+ add(self, object, /) unbound builtins.set method


======================================================================
FAIL: test__get_version (__main__.TestInternalUtilities.test__get_version)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pydoc.py", line 2482, in test__get_version
    self.assertEqual(len(w), 0)
AssertionError: 1 != 0

----------------------------------------------------------------------
Ran 120 tests in Ns

FAILED (failures=33, errors=13, skipped=37)
