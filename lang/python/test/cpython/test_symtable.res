EEEE.EEEEEE...FE.E....FE.....F..F...........E...
======================================================================
ERROR: test_eval (__main__.ASTInputTests.test_eval)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 571, in test_eval
    table = symtable.symtable(ast.parse("a + b", mode="eval"), "?", "eval")
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_exec (__main__.ASTInputTests.test_exec)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 567, in test_exec
    top = symtable.symtable(ast.parse(TEST_CODE), "?", "exec")
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_invalid_ast (__main__.ASTInputTests.test_invalid_ast)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 619, in test_invalid_ast
    symtable.symtable(node, "?", "eval")
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_misplaced_future_import (__main__.ASTInputTests.test_misplaced_future_import)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 626, in test_misplaced_future_import
    symtable.symtable(tree, "?", "exec")
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_same_result_as_string (__main__.ASTInputTests.test_same_result_as_string) (source='\nimport sys\n\nglob = 42\nsome_var = 12\nsome_non_assigned_global_var: int\nsome_assigned_global_var = 11\n\nclass Mine:\n    instance_var = 24\n    def a_method(p1, p2):\n        pass\n\ndef spam(a, b, *var, **kw):\n    global bar\n    global some_assigned_global_var\n    some_assigned_global_var = 12\n    bar = 47\n    some_var = 10\n    x = 23\n    glob\n    def internal():\n        return x\n    def other_internal():\n        nonlocal some_var\n        some_var = 3\n        return some_var\n    return internal\n\ndef foo():\n    pass\n\ndef namespace_test(): pass\ndef namespace_test(): pass\n\ntype Alias = int\ntype GenericAlias[T] = list[T]\n\ndef generic_spam[T](a):\n    pass\n\nclass GenericMine[T: int, U: (int, str) = int]:\n    pass\n', mode='exec')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 590, in test_same_result_as_string
    from_ast = symtable.symtable(ast.parse(source, mode=mode),
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_same_result_as_string (__main__.ASTInputTests.test_same_result_as_string) (source='from __future__ import annotations\ndef f(x: int) -> int: return x\n', mode='exec')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 590, in test_same_result_as_string
    from_ast = symtable.symtable(ast.parse(source, mode=mode),
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_same_result_as_string (__main__.ASTInputTests.test_same_result_as_string) (source='[x*y for x in a]', mode='eval')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 590, in test_same_result_as_string
    from_ast = symtable.symtable(ast.parse(source, mode=mode),
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_same_result_as_string (__main__.ASTInputTests.test_same_result_as_string) (source='def f(): pass\n', mode='single')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 590, in test_same_result_as_string
    from_ast = symtable.symtable(ast.parse(source, mode=mode),
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_single (__main__.ASTInputTests.test_single)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 575, in test_single
    table = symtable.symtable(ast.parse("x = 1", mode="single"),
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_synthesized_ast (__main__.ASTInputTests.test_synthesized_ast)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 602, in test_synthesized_ast
    top = symtable.symtable(node, "?", "exec")
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
TypeError: symtable() argument 1 must be str or bytes

======================================================================
ERROR: test_annotated (__main__.SymtableTest.test_annotated)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 332, in test_annotated
    st2 = st1.get_children()[1]
IndexError: list index out of range

======================================================================
ERROR: test_bytes (__main__.SymtableTest.test_bytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 403, in test_bytes
    top = symtable.symtable(code, "?", "exec")
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
  File "<string>", line 0
SyntaxError: unknown encoding: iso8859-15

======================================================================
ERROR: test_filter_syntax_warnings_by_module (__main__.SymtableTest.test_filter_syntax_warnings_by_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 514, in test_filter_syntax_warnings_by_module
    with open(filename, 'rb') as f:
FileNotFoundError: [Errno 2] No such file or directory: 'test_import/data/syntax_warnings.py'

======================================================================
ERROR: test_symbol_repr (__main__.SymtableTest.test_symbol_repr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 429, in test_symbol_repr
    self.assertEqual(repr(st1.lookup("x")),
  File "/pkg/store/python-0/lib/symtable.py", line 151, in lookup
    flags = self._table.symbols[name]
KeyError: 'x'

======================================================================
FAIL: test__symtable_refleak (__main__.SymtableTest.test__symtable_refleak)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 510, in test__symtable_refleak
    self.assertRaises(TypeError, symtable.symtable, '', mortal_str, 1)
AssertionError: TypeError not raised by symtable

======================================================================
FAIL: test_filename_correct (__main__.SymtableTest.test_filename_correct)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 371, in checkfilename
    symtable.symtable(brokencode, "spam", "exec")
  File "/pkg/store/python-0/lib/symtable.py", line 27, in symtable
    top = _symtable.symtable(code, filename, compile_type, module=module)
  File "<string>", line 1
SyntaxError: unmatched bracket

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 378, in test_filename_correct
    checkfilename("def f(x): foo)(", 14)  # parse-time
  File "/tmp/test_symtable.py", line 373, in checkfilename
    self.assertEqual(e.filename, "spam")
AssertionError: None != 'spam'

======================================================================
FAIL: test_imported (__main__.SymtableTest.test_imported)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 358, in test_imported
    self.assertTrue(self.top.lookup("sys").is_imported())
AssertionError: False is not true

======================================================================
FAIL: test_local (__main__.SymtableTest.test_local)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_symtable.py", line 281, in test_local
    self.assertTrue(self.top.lookup("some_non_assigned_global_var").is_local())
AssertionError: False is not true

----------------------------------------------------------------------
Ran 45 tests in Ns

FAILED (failures=4, errors=14)
