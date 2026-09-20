.Fs.......ssFFFsFFFFFFF.FFFFF..........F...FFFss.F..s.s..Fs.......ssFFFsFFFFFFF.FFFFF.....FFFss.F..s.s.FFF.FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF.
======================================================================
FAIL: test_assign_del (__main__.LazyImportRestrictionTestCase.test_assign_del)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    del 1
        ^
SyntaxError: cannot assign to this

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3096, in test_assign_del
    self._check_error("del 1", "cannot delete literal")
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'cannot delete literal'

======================================================================
FAIL: test_double_ampersand (__main__.LazyImportRestrictionTestCase.test_double_ampersand)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    a && b
       ^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3631, in test_double_ampersand
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "Maybe you meant 'and' or '&' instead of '&&'\\?"

======================================================================
FAIL: test_double_pipe (__main__.LazyImportRestrictionTestCase.test_double_pipe)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    a || b
       ^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3657, in test_double_pipe
    self._check_error(
  File "/tmp/test_syntax.py", line 3072, in _check_error
    self.assertEqual(err.offset, offset)
AssertionError: 4 != 3

======================================================================
FAIL: test_empty_line_after_linecont (__main__.LazyImportRestrictionTestCase.test_empty_line_after_linecont)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3253, in test_empty_line_after_linecont
    compile(s, '<string>', 'exec')
  File "<string>", line 3
    \
IndentationError: unexpected indent

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3255, in test_empty_line_after_linecont
    self.fail("Empty line after a line continuation character is valid.")
AssertionError: Empty line after a line continuation character is valid.

======================================================================
FAIL: test_error_parenthesis (__main__.LazyImportRestrictionTestCase.test_error_parenthesis)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    (1 + 2
          ^
SyntaxError: unexpected EOF while parsing

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3411, in test_error_parenthesis
    self._check_error(paren + "1 + 2", f"\\{paren}' was never closed")
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "\\(' was never closed"

======================================================================
FAIL: test_error_string_literal (__main__.LazyImportRestrictionTestCase.test_error_string_literal)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    'blech
    ^^^^^^
SyntaxError: EOL while scanning string literal

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3436, in test_error_string_literal
    self._check_error("'blech", r"unterminated string literal \(.*\)$")
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'unterminated string literal \\(.*\\)$'

======================================================================
FAIL: test_except_star_then_except (__main__.LazyImportRestrictionTestCase.test_except_star_then_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    except TypeError: pass
           ^^^^^^^^^^^^^^^
SyntaxError: cannot have both 'except' and 'except*' on the same 'try'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3240, in test_except_star_then_except
    self._check_error("try: pass\nexcept* ValueError: pass\nexcept TypeError: pass",
  File "/tmp/test_syntax.py", line 3072, in _check_error
    self.assertEqual(err.offset, offset)
AssertionError: 8 != 1

======================================================================
FAIL: test_except_stmt_invalid_as_expr (__main__.LazyImportRestrictionTestCase.test_except_stmt_invalid_as_expr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 4
    except ValueError as obj.attr:
                            ^^^^^^
SyntaxError: expected ':'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3559, in test_except_stmt_invalid_as_expr
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'cannot use except statement with attribute'

======================================================================
FAIL: test_except_then_except_star (__main__.LazyImportRestrictionTestCase.test_except_then_except_star)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    except* TypeError: pass
            ^^^^^^^^^^^^^^^
SyntaxError: cannot have both 'except' and 'except*' on the same 'try'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3235, in test_except_then_except_star
    self._check_error("try: pass\nexcept ValueError: pass\nexcept* TypeError: pass",
  File "/tmp/test_syntax.py", line 3072, in _check_error
    self.assertEqual(err.offset, offset)
AssertionError: 9 != 1

======================================================================
FAIL: test_expression_with_assignment (__main__.LazyImportRestrictionTestCase.test_expression_with_assignment)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    print(end1 + end2 = ' ')
                      ^^^^^^
SyntaxError: expected ')'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3082, in test_expression_with_assignment
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'expression cannot contain assignment, perhaps you meant "=="?'

======================================================================
FAIL: test_generator_in_function_call (__main__.LazyImportRestrictionTestCase.test_generator_in_function_call)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3230, in test_generator_in_function_call
    self._check_error("foo(x,    y for y in range(3) for z in range(2) if z    , p)",
  File "/tmp/test_syntax.py", line 3079, in _check_error
    self.fail("compile() did not raise SyntaxError")
AssertionError: compile() did not raise SyntaxError

======================================================================
FAIL: test_ifexp_body_stmt_else_expression (__main__.LazyImportRestrictionTestCase.test_ifexp_body_stmt_else_expression)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    x = pass if 1 else 1
        ^^^^^^^^^^^^^^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3619, in test_ifexp_body_stmt_else_expression
    self._check_error(f"x = {stmt} if 1 else 1", msg)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "expected expression before 'if', but statement is given"

======================================================================
FAIL: test_ifexp_body_stmt_else_stmt (__main__.LazyImportRestrictionTestCase.test_ifexp_body_stmt_else_stmt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    x = pass if 1 else pass
        ^^^^^^^^^^^^^^^^^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3628, in test_ifexp_body_stmt_else_stmt
    self._check_error(f"x = {lhs_stmt} if 1 else {rhs_stmt}", msg)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "expected expression before 'if', but statement is given"

======================================================================
FAIL: test_ifexp_else_stmt (__main__.LazyImportRestrictionTestCase.test_ifexp_else_stmt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    x = 1 if 1 else pass
                    ^^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3609, in test_ifexp_else_stmt
    self._check_error(f"x = 1 if 1 else {stmt}", msg)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "expected expression after 'else', but statement is given"

======================================================================
FAIL: test_invalid_line_continuation_error_position (__main__.LazyImportRestrictionTestCase.test_invalid_line_continuation_error_position)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    a = 3 \ 4
          ^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3387, in test_invalid_line_continuation_error_position
    self._check_error(r"a = 3 \ 4",
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'unexpected character after line continuation character'

======================================================================
FAIL: test_invalid_line_continuation_left_recursive (__main__.LazyImportRestrictionTestCase.test_invalid_line_continuation_left_recursive)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    A.Ɗ\ 
       ^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3404, in test_invalid_line_continuation_left_recursive
    self._check_error("A.\u018a\\ ",
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'unexpected character after line continuation character'

======================================================================
FAIL: test_lazy_import_nested_scopes (__main__.LazyImportRestrictionTestCase.test_lazy_import_nested_scopes)
Test lazy imports in nested scopes.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    from os lazy import path
            ^^^^^^^^^^^^^^^^
SyntaxError: expected 'import'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3787, in test_lazy_import_nested_scopes
    self._check_error("""\
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "use 'lazy from ... ' instead of 'from ... lazy import'"

======================================================================
FAIL: test_match_stmt_invalid_as_expr (__main__.LazyImportRestrictionTestCase.test_match_stmt_invalid_as_expr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    case x as obj.attr:
                 ^^^^^^
SyntaxError: expected ':'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3576, in test_match_stmt_invalid_as_expr
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'cannot use attribute as pattern target'

======================================================================
FAIL: test_multiline_compiler_error_points_to_the_end (__main__.LazyImportRestrictionTestCase.test_multiline_compiler_error_points_to_the_end)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3470, in test_multiline_compiler_error_points_to_the_end
    self._check_error(
  File "/tmp/test_syntax.py", line 3079, in _check_error
    self.fail("compile() did not raise SyntaxError")
AssertionError: compile() did not raise SyntaxError

======================================================================
FAIL: test_multiline_string_concat_missing_comma_points_to_last_string (__main__.LazyImportRestrictionTestCase.test_multiline_string_concat_missing_comma_points_to_last_string)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 5
    x=1
    ^^^
SyntaxError: expected ')'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3479, in test_multiline_string_concat_missing_comma_points_to_last_string
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'Perhaps you forgot a comma'

======================================================================
FAIL: test_nonlocal_param_err_first (__main__.LazyImportRestrictionTestCase.test_nonlocal_param_err_first)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    nonlocal a  # SyntaxError
             ^^^^^^^^^^^^^^^^
SyntaxError: name is parameter and global

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3143, in test_nonlocal_param_err_first
    self._check_error(source, "parameter and nonlocal", lineno=3)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'parameter and nonlocal'

======================================================================
FAIL: test_assign_del (__main__.SyntaxErrorTestCase.test_assign_del)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    del 1
        ^
SyntaxError: cannot assign to this

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3096, in test_assign_del
    self._check_error("del 1", "cannot delete literal")
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'cannot delete literal'

======================================================================
FAIL: test_double_ampersand (__main__.SyntaxErrorTestCase.test_double_ampersand)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    a && b
       ^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3631, in test_double_ampersand
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "Maybe you meant 'and' or '&' instead of '&&'\\?"

======================================================================
FAIL: test_double_pipe (__main__.SyntaxErrorTestCase.test_double_pipe)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    a || b
       ^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3657, in test_double_pipe
    self._check_error(
  File "/tmp/test_syntax.py", line 3072, in _check_error
    self.assertEqual(err.offset, offset)
AssertionError: 4 != 3

======================================================================
FAIL: test_empty_line_after_linecont (__main__.SyntaxErrorTestCase.test_empty_line_after_linecont)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3253, in test_empty_line_after_linecont
    compile(s, '<string>', 'exec')
  File "<string>", line 3
    \
IndentationError: unexpected indent

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3255, in test_empty_line_after_linecont
    self.fail("Empty line after a line continuation character is valid.")
AssertionError: Empty line after a line continuation character is valid.

======================================================================
FAIL: test_error_parenthesis (__main__.SyntaxErrorTestCase.test_error_parenthesis)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    (1 + 2
          ^
SyntaxError: unexpected EOF while parsing

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3411, in test_error_parenthesis
    self._check_error(paren + "1 + 2", f"\\{paren}' was never closed")
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "\\(' was never closed"

======================================================================
FAIL: test_error_string_literal (__main__.SyntaxErrorTestCase.test_error_string_literal)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    'blech
    ^^^^^^
SyntaxError: EOL while scanning string literal

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3436, in test_error_string_literal
    self._check_error("'blech", r"unterminated string literal \(.*\)$")
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'unterminated string literal \\(.*\\)$'

======================================================================
FAIL: test_except_star_then_except (__main__.SyntaxErrorTestCase.test_except_star_then_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    except TypeError: pass
           ^^^^^^^^^^^^^^^
SyntaxError: cannot have both 'except' and 'except*' on the same 'try'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3240, in test_except_star_then_except
    self._check_error("try: pass\nexcept* ValueError: pass\nexcept TypeError: pass",
  File "/tmp/test_syntax.py", line 3072, in _check_error
    self.assertEqual(err.offset, offset)
AssertionError: 8 != 1

======================================================================
FAIL: test_except_stmt_invalid_as_expr (__main__.SyntaxErrorTestCase.test_except_stmt_invalid_as_expr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 4
    except ValueError as obj.attr:
                            ^^^^^^
SyntaxError: expected ':'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3559, in test_except_stmt_invalid_as_expr
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'cannot use except statement with attribute'

======================================================================
FAIL: test_except_then_except_star (__main__.SyntaxErrorTestCase.test_except_then_except_star)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    except* TypeError: pass
            ^^^^^^^^^^^^^^^
SyntaxError: cannot have both 'except' and 'except*' on the same 'try'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3235, in test_except_then_except_star
    self._check_error("try: pass\nexcept ValueError: pass\nexcept* TypeError: pass",
  File "/tmp/test_syntax.py", line 3072, in _check_error
    self.assertEqual(err.offset, offset)
AssertionError: 9 != 1

======================================================================
FAIL: test_expression_with_assignment (__main__.SyntaxErrorTestCase.test_expression_with_assignment)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    print(end1 + end2 = ' ')
                      ^^^^^^
SyntaxError: expected ')'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3082, in test_expression_with_assignment
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'expression cannot contain assignment, perhaps you meant "=="?'

======================================================================
FAIL: test_generator_in_function_call (__main__.SyntaxErrorTestCase.test_generator_in_function_call)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3230, in test_generator_in_function_call
    self._check_error("foo(x,    y for y in range(3) for z in range(2) if z    , p)",
  File "/tmp/test_syntax.py", line 3079, in _check_error
    self.fail("compile() did not raise SyntaxError")
AssertionError: compile() did not raise SyntaxError

======================================================================
FAIL: test_ifexp_body_stmt_else_expression (__main__.SyntaxErrorTestCase.test_ifexp_body_stmt_else_expression)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    x = pass if 1 else 1
        ^^^^^^^^^^^^^^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3619, in test_ifexp_body_stmt_else_expression
    self._check_error(f"x = {stmt} if 1 else 1", msg)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "expected expression before 'if', but statement is given"

======================================================================
FAIL: test_ifexp_body_stmt_else_stmt (__main__.SyntaxErrorTestCase.test_ifexp_body_stmt_else_stmt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    x = pass if 1 else pass
        ^^^^^^^^^^^^^^^^^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3628, in test_ifexp_body_stmt_else_stmt
    self._check_error(f"x = {lhs_stmt} if 1 else {rhs_stmt}", msg)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "expected expression before 'if', but statement is given"

======================================================================
FAIL: test_ifexp_else_stmt (__main__.SyntaxErrorTestCase.test_ifexp_else_stmt)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    x = 1 if 1 else pass
                    ^^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3609, in test_ifexp_else_stmt
    self._check_error(f"x = 1 if 1 else {stmt}", msg)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain "expected expression after 'else', but statement is given"

======================================================================
FAIL: test_invalid_line_continuation_error_position (__main__.SyntaxErrorTestCase.test_invalid_line_continuation_error_position)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    a = 3 \ 4
          ^^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3387, in test_invalid_line_continuation_error_position
    self._check_error(r"a = 3 \ 4",
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'unexpected character after line continuation character'

======================================================================
FAIL: test_invalid_line_continuation_left_recursive (__main__.SyntaxErrorTestCase.test_invalid_line_continuation_left_recursive)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 1
    A.Ɗ\ 
       ^^
SyntaxError: invalid syntax

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3404, in test_invalid_line_continuation_left_recursive
    self._check_error("A.\u018a\\ ",
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'unexpected character after line continuation character'

======================================================================
FAIL: test_match_stmt_invalid_as_expr (__main__.SyntaxErrorTestCase.test_match_stmt_invalid_as_expr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    case x as obj.attr:
                 ^^^^^^
SyntaxError: expected ':'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3576, in test_match_stmt_invalid_as_expr
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'cannot use attribute as pattern target'

======================================================================
FAIL: test_multiline_compiler_error_points_to_the_end (__main__.SyntaxErrorTestCase.test_multiline_compiler_error_points_to_the_end)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3470, in test_multiline_compiler_error_points_to_the_end
    self._check_error(
  File "/tmp/test_syntax.py", line 3079, in _check_error
    self.fail("compile() did not raise SyntaxError")
AssertionError: compile() did not raise SyntaxError

======================================================================
FAIL: test_multiline_string_concat_missing_comma_points_to_last_string (__main__.SyntaxErrorTestCase.test_multiline_string_concat_missing_comma_points_to_last_string)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 5
    x=1
    ^^^
SyntaxError: expected ')'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3479, in test_multiline_string_concat_missing_comma_points_to_last_string
    self._check_error(
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'Perhaps you forgot a comma'

======================================================================
FAIL: test_nonlocal_param_err_first (__main__.SyntaxErrorTestCase.test_nonlocal_param_err_first)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3061, in _check_error
    compile(code, filename, mode)
  File "<testcase>", line 3
    nonlocal a  # SyntaxError
             ^^^^^^^^^^^^^^^^
SyntaxError: name is parameter and global

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3143, in test_nonlocal_param_err_first
    self._check_error(source, "parameter and nonlocal", lineno=3)
  File "/tmp/test_syntax.py", line 3067, in _check_error
    self.fail("SyntaxError did not contain %r" % (errtext,))
AssertionError: SyntaxError did not contain 'parameter and nonlocal'

======================================================================
FAIL: test_break_and_continue_in_finally (__main__.SyntaxWarningTest.test_break_and_continue_in_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2953, in test_break_and_continue_in_finally
    self.check_warning(source, f"'{kw}' in a 'finally' block")
  File "/tmp/test_syntax.py", line 2898, in check_warning
    with self.assertWarnsRegex(SyntaxWarning, errtext):
AssertionError: SyntaxWarning not triggered

======================================================================
FAIL: test_from_lazy_imports (__main__.SyntaxWarningTest.test_from_lazy_imports)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2981, in test_from_lazy_imports
    self.check_warning(
  File "/tmp/test_syntax.py", line 2898, in check_warning
    with self.assertWarnsRegex(SyntaxWarning, errtext):
AssertionError: SyntaxWarning not triggered

======================================================================
FAIL: test_from_lazy_imports_as_error (__main__.SyntaxWarningTest.test_from_lazy_imports_as_error)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 3039, in test_from_lazy_imports_as_error
    with self.assertRaisesRegex(
AssertionError: SyntaxError not raised

======================================================================
FAIL: test_return_in_finally (__main__.SyntaxWarningTest.test_return_in_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2917, in test_return_in_finally
    self.check_warning(source, "'return' in a 'finally' block")
  File "/tmp/test_syntax.py", line 2898, in check_warning
    with self.assertWarnsRegex(SyntaxWarning, errtext):
AssertionError: SyntaxWarning not triggered

======================================================================
FAIL: __main__ () [0]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 5, in __main__
    >>> def f(x):
AssertionError: Failed example:
    def f(x):
        global x
Expected:
    Traceback (most recent call last):
    SyntaxError: name 'x' is parameter and global
Got:
      File "<doctest __main__[0]>", line 2
        global x
               ^
    SyntaxError: name is parameter and global

======================================================================
FAIL: __main__ () [1]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 30, in __main__
    >>> obj.None = 1
AssertionError: Failed example:
    obj.None = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[1]>", line 1
        obj.None = 1
            ^^^^^^^^
    SyntaxError: expected a name after '.'

======================================================================
FAIL: __main__ () [2]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 34, in __main__
    >>> None = 1
AssertionError: Failed example:
    None = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to None
Got:
      File "<doctest __main__[2]>", line 1
        None = 1
        ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [3]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 38, in __main__
    >>> obj.True = 1
AssertionError: Failed example:
    obj.True = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[3]>", line 1
        obj.True = 1
            ^^^^^^^^
    SyntaxError: expected a name after '.'

======================================================================
FAIL: __main__ () [4]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 42, in __main__
    >>> True = 1
AssertionError: Failed example:
    True = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to True
Got:
      File "<doctest __main__[4]>", line 1
        True = 1
        ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [13]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 78, in __main__
    >>> f() = 1
AssertionError: Failed example:
    f() = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[13]>", line 1
        f() = 1
         ^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [14]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 82, in __main__
    >>> yield = 1
AssertionError: Failed example:
    yield = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: assignment to yield expression not possible
Got:
      File "<doctest __main__[14]>", line 1
        yield = 1
              ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [15]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 86, in __main__
    >>> del f()
AssertionError: Failed example:
    del f()
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot delete function call
Got:
      File "<doctest __main__[15]>", line 1
        del f()
             ^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [16]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 90, in __main__
    >>> a + 1 = 2
AssertionError: Failed example:
    a + 1 = 2
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[16]>", line 1
        a + 1 = 2
          ^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [17]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 94, in __main__
    >>> (x for x in x) = 1
AssertionError: Failed example:
    (x for x in x) = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to generator expression
Got:
      File "<doctest __main__[17]>", line 1
        (x for x in x) = 1
        ^^^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [18]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 98, in __main__
    >>> 1 = 1
AssertionError: Failed example:
    1 = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[18]>", line 1
        1 = 1
        ^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [19]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 102, in __main__
    >>> "abc" = 1
AssertionError: Failed example:
    "abc" = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[19]>", line 1
        "abc" = 1
        ^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [20]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 106, in __main__
    >>> b"" = 1
AssertionError: Failed example:
    b"" = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[20]>", line 1
        b"" = 1
        ^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [21]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 110, in __main__
    >>> ... = 1
AssertionError: Failed example:
    ... = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to ellipsis here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[21]>", line 1
        ... = 1
        ^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [23]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 123, in __main__
    >>> (a, "b", c) = (1, 2, 3)
AssertionError: Failed example:
    (a, "b", c) = (1, 2, 3)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal
Got:
      File "<doctest __main__[23]>", line 1
        (a, "b", c) = (1, 2, 3)
            ^^^^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [24]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 127, in __main__
    >>> (a, True, c) = (1, 2, 3)
AssertionError: Failed example:
    (a, True, c) = (1, 2, 3)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to True
Got:
      File "<doctest __main__[24]>", line 1
        (a, True, c) = (1, 2, 3)
            ^^^^^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [26]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 135, in __main__
    >>> (a, *True, c) = (1, 2, 3)
AssertionError: Failed example:
    (a, *True, c) = (1, 2, 3)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to True
Got:
      File "<doctest __main__[26]>", line 1
        (a, *True, c) = (1, 2, 3)
             ^^^^^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [28]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 143, in __main__
    >>> [a, b, c + 1] = [1, 2, 3]
AssertionError: Failed example:
    [a, b, c + 1] = [1, 2, 3]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[28]>", line 1
        [a, b, c + 1] = [1, 2, 3]
                 ^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [29]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 147, in __main__
    >>> [a, b[1], c + 1] = [1, 2, 3]
AssertionError: Failed example:
    [a, b[1], c + 1] = [1, 2, 3]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[29]>", line 1
        [a, b[1], c + 1] = [1, 2, 3]
                    ^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [30]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 151, in __main__
    >>> [a, b.c.d, c + 1] = [1, 2, 3]
AssertionError: Failed example:
    [a, b.c.d, c + 1] = [1, 2, 3]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[30]>", line 1
        [a, b.c.d, c + 1] = [1, 2, 3]
                     ^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [31]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 155, in __main__
    >>> a if 1 else b = 1
AssertionError: Failed example:
    a if 1 else b = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to conditional expression
Got:
      File "<doctest __main__[31]>", line 1
        a if 1 else b = 1
          ^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [32]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 159, in __main__
    >>> a = 42 if True
AssertionError: Failed example:
    a = 42 if True
Expected:
    Traceback (most recent call last):
    SyntaxError: expected 'else' after 'if' expression
Got:
      File "<doctest __main__[32]>", line 1
        a = 42 if True
                      ^
    SyntaxError: expected 'else'

======================================================================
FAIL: __main__ () [33]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 163, in __main__
    >>> a = (42 if True)
AssertionError: Failed example:
    a = (42 if True)
Expected:
    Traceback (most recent call last):
    SyntaxError: expected 'else' after 'if' expression
Got:
      File "<doctest __main__[33]>", line 1
        a = (42 if True)
                       ^
    SyntaxError: expected 'else'

======================================================================
FAIL: __main__ () [34]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 167, in __main__
    >>> a = [1, 42 if True, 4]
AssertionError: Failed example:
    a = [1, 42 if True, 4]
Expected:
    Traceback (most recent call last):
    SyntaxError: expected 'else' after 'if' expression
Got:
      File "<doctest __main__[34]>", line 1
        a = [1, 42 if True, 4]
                          ^^^^
    SyntaxError: expected 'else'

======================================================================
FAIL: __main__ () [35]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 171, in __main__
    >>> x = 1 if 1 else pass
AssertionError: Failed example:
    x = 1 if 1 else pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected expression after 'else', but statement is given
Got:
      File "<doctest __main__[35]>", line 1
        x = 1 if 1 else pass
                        ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [36]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 175, in __main__
    >>> x = pass if 1 else 1
AssertionError: Failed example:
    x = pass if 1 else 1
Expected:
    Traceback (most recent call last):
    SyntaxError: expected expression before 'if', but statement is given
Got:
      File "<doctest __main__[36]>", line 1
        x = pass if 1 else 1
            ^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [37]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 179, in __main__
    >>> x = pass if 1 else pass
AssertionError: Failed example:
    x = pass if 1 else pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected expression before 'if', but statement is given
Got:
      File "<doctest __main__[37]>", line 1
        x = pass if 1 else pass
            ^^^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [38]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 183, in __main__
    >>> if True:
AssertionError: Failed example:
    if True:
        print("Hello"

    if 2:
       print(123))
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[38]>", line 4
        if 2:
            ^
    SyntaxError: expected 'else'

======================================================================
FAIL: __main__ () [39]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 191, in __main__
    >>> True = True = 3
AssertionError: Failed example:
    True = True = 3
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to True
Got:
      File "<doctest __main__[39]>", line 1
        True = True = 3
        ^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [40]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 195, in __main__
    >>> x = y = True = z = 3
AssertionError: Failed example:
    x = y = True = z = 3
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to True
Got:
      File "<doctest __main__[40]>", line 1
        x = y = True = z = 3
                ^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [41]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 199, in __main__
    >>> x = y = yield = 1
AssertionError: Failed example:
    x = y = yield = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: assignment to yield expression not possible
Got:
      File "<doctest __main__[41]>", line 1
        x = y = yield = 1
                      ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [42]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 203, in __main__
    >>> a, b += 1, 2
AssertionError: Failed example:
    a, b += 1, 2
Expected:
    Traceback (most recent call last):
    SyntaxError: 'tuple' is an illegal expression for augmented assignment
Got:
      File "<doctest __main__[42]>", line 1
        a, b += 1, 2
        ^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [43]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 207, in __main__
    >>> (a, b) += 1, 2
AssertionError: Failed example:
    (a, b) += 1, 2
Expected:
    Traceback (most recent call last):
    SyntaxError: 'tuple' is an illegal expression for augmented assignment
Got:
      File "<doctest __main__[43]>", line 1
        (a, b) += 1, 2
        ^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [44]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 211, in __main__
    >>> [a, b] += 1, 2
AssertionError: Failed example:
    [a, b] += 1, 2
Expected:
    Traceback (most recent call last):
    SyntaxError: 'list' is an illegal expression for augmented assignment
Got:
      File "<doctest __main__[44]>", line 1
        [a, b] += 1, 2
        ^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [45]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 218, in __main__
    >>> for a() in b: pass
AssertionError: Failed example:
    for a() in b: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[45]>", line 1
        for a() in b: pass
             ^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [46]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 222, in __main__
    >>> for (a, b()) in b: pass
AssertionError: Failed example:
    for (a, b()) in b: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[46]>", line 1
        for (a, b()) in b: pass
                 ^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [47]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 226, in __main__
    >>> for [a, b()] in b: pass
AssertionError: Failed example:
    for [a, b()] in b: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[47]>", line 1
        for [a, b()] in b: pass
                 ^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [48]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 230, in __main__
    >>> for (*a, b, c+1) in b: pass
AssertionError: Failed example:
    for (*a, b, c+1) in b: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[48]>", line 1
        for (*a, b, c+1) in b: pass
                     ^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [49]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 234, in __main__
    >>> for (x, *(y, z.d())) in b: pass
AssertionError: Failed example:
    for (x, *(y, z.d())) in b: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[49]>", line 1
        for (x, *(y, z.d())) in b: pass
                        ^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [50]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 238, in __main__
    >>> for a, b() in c: pass
AssertionError: Failed example:
    for a, b() in c: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[50]>", line 1
        for a, b() in c: pass
                ^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [51]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 242, in __main__
    >>> for a, b, (c + 1, d()): pass
AssertionError: Failed example:
    for a, b, (c + 1, d()): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[51]>", line 1
        for a, b, (c + 1, d()): pass
                              ^^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [52]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 246, in __main__
    >>> for i < (): pass
AssertionError: Failed example:
    for i < (): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[52]>", line 1
        for i < (): pass
              ^^^^^^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [53]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 250, in __main__
    >>> for a, b
AssertionError: Failed example:
    for a, b
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[53]>", line 1
        for a, b
                ^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [54]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 254, in __main__
    >>> with a as b(): pass
AssertionError: Failed example:
    with a as b(): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[54]>", line 1
        with a as b(): pass
                   ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [55]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 258, in __main__
    >>> with a as (b, c()): pass
AssertionError: Failed example:
    with a as (b, c()): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[55]>", line 1
        with a as (b, c()): pass
                       ^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [56]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 262, in __main__
    >>> with a as [b, c()]: pass
AssertionError: Failed example:
    with a as [b, c()]: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[56]>", line 1
        with a as [b, c()]: pass
                       ^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [57]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 266, in __main__
    >>> with a as (*b, c, d+1): pass
AssertionError: Failed example:
    with a as (*b, c, d+1): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[57]>", line 1
        with a as (*b, c, d+1): pass
                           ^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [58]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 270, in __main__
    >>> with a as (x, *(y, z.d())): pass
AssertionError: Failed example:
    with a as (x, *(y, z.d())): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[58]>", line 1
        with a as (x, *(y, z.d())): pass
                              ^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [59]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 274, in __main__
    >>> with a as b, c as d(): pass
AssertionError: Failed example:
    with a as b, c as d(): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[59]>", line 1
        with a as b, c as d(): pass
                           ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [62]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 288, in __main__
    >>> [x for x if range(1)]
AssertionError: Failed example:
    [x for x if range(1)]
Expected:
    Traceback (most recent call last):
    SyntaxError: 'in' expected after for-loop variables
Got:
      File "<doctest __main__[62]>", line 1
        [x for x if range(1)]
                 ^^^^^^^^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [63]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 292, in __main__
    >>> tuple(x for x if range(1))
AssertionError: Failed example:
    tuple(x for x if range(1))
Expected:
    Traceback (most recent call last):
    SyntaxError: 'in' expected after for-loop variables
Got:
      File "<doctest __main__[63]>", line 1
        tuple(x for x if range(1))
                      ^^^^^^^^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [64]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 296, in __main__
    >>> [x for x() in a]
AssertionError: Failed example:
    [x for x() in a]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to function call
Got:
      File "<doctest __main__[64]>", line 1
        [x for x() in a]
                ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [65]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 300, in __main__
    >>> [x for a, b, (c + 1, d()) in y]
AssertionError: Failed example:
    [x for a, b, (c + 1, d()) in y]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[65]>", line 1
        [x for a, b, (c + 1, d()) in y]
                        ^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [66]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 304, in __main__
    >>> [x for a, b, (c + 1, d()) if y]
AssertionError: Failed example:
    [x for a, b, (c + 1, d()) if y]
Expected:
    Traceback (most recent call last):
    SyntaxError: 'in' expected after for-loop variables
Got:
      File "<doctest __main__[66]>", line 1
        [x for a, b, (c + 1, d()) if y]
                                  ^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [67]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 308, in __main__
    >>> [x for x+1 in y]
AssertionError: Failed example:
    [x for x+1 in y]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[67]>", line 1
        [x for x+1 in y]
                ^^^^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [68]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 312, in __main__
    >>> [x for x+1, x() in y]
AssertionError: Failed example:
    [x for x+1, x() in y]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to expression
Got:
      File "<doctest __main__[68]>", line 1
        [x for x+1, x() in y]
                ^^^^^^^^^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [71]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 329, in __main__
    >>> "The interesting object "The important object" is very important"
AssertionError: Failed example:
    "The interesting object "The important object" is very important"
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Is this intended to be part of the string?
Got:
      File "<doctest __main__[71]>", line 1
        "The interesting object "The important object" is very important"
                                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [72]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 337, in __main__
    >>> [1, 2 3]
AssertionError: Failed example:
    [1, 2 3]
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Perhaps you forgot a comma?
Got:
      File "<doctest __main__[72]>", line 1
        [1, 2 3]
              ^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [73]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 341, in __main__
    >>> {1, 2 3}
AssertionError: Failed example:
    {1, 2 3}
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Perhaps you forgot a comma?
Got:
      File "<doctest __main__[73]>", line 1
        {1, 2 3}
              ^^
    SyntaxError: expected '}'

======================================================================
FAIL: __main__ () [74]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 345, in __main__
    >>> {1:2, 2:5 3:12}
AssertionError: Failed example:
    {1:2, 2:5 3:12}
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Perhaps you forgot a comma?
Got:
      File "<doctest __main__[74]>", line 1
        {1:2, 2:5 3:12}
                  ^^^^^
    SyntaxError: expected '}'

======================================================================
FAIL: __main__ () [75]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 349, in __main__
    >>> (1, 2 3)
AssertionError: Failed example:
    (1, 2 3)
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Perhaps you forgot a comma?
Got:
      File "<doctest __main__[75]>", line 1
        (1, 2 3)
              ^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [79]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 373, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
        case {**_}:
           ...
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[79]>", line 2
        case {**_}:
                ^^^
    SyntaxError: cannot use '_' as a target

======================================================================
FAIL: __main__ () [80]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 381, in __main__
    >>> case "pattern": ...
AssertionError: Failed example:
    case "pattern": ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[80]>", line 1
        case "pattern": ...
             ^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [81]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 385, in __main__
    >>> case 1 | 2: ...
AssertionError: Failed example:
    case 1 | 2: ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[81]>", line 1
        case 1 | 2: ...
             ^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [82]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 389, in __main__
    >>> case klass(attr=1) | {}: ...
AssertionError: Failed example:
    case klass(attr=1) | {}: ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[82]>", line 1
        case klass(attr=1) | {}: ...
             ^^^^^^^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [83]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 393, in __main__
    >>> case [] if x > 1: ...
AssertionError: Failed example:
    case [] if x > 1: ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[83]>", line 1
        case [] if x > 1: ...
              ^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [84]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 397, in __main__
    >>> case match: ...
AssertionError: Failed example:
    case match: ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[84]>", line 1
        case match: ...
             ^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [85]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 401, in __main__
    >>> case case: ...
AssertionError: Failed example:
    case case: ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[85]>", line 1
        case case: ...
             ^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [86]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 405, in __main__
    >>> if some:
AssertionError: Failed example:
    if some:
        case 1: ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[86]>", line 2
        case 1: ...
             ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [87]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 410, in __main__
    >>> case some:
AssertionError: Failed example:
    case some:
        case 1: ...
Expected:
    Traceback (most recent call last):
    SyntaxError: case statement must be inside match statement
Got:
      File "<doctest __main__[87]>", line 1
        case some:
             ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [88]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 418, in __main__
    >>> (mat x)
AssertionError: Failed example:
    (mat x)
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Perhaps you forgot a comma?
Got:
      File "<doctest __main__[88]>", line 1
        (mat x)
             ^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [89]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 424, in __main__
    >>> def f(None=1):
AssertionError: Failed example:
    def f(None=1):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[89]>", line 1
        def f(None=1):
              ^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [90]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 431, in __main__
    >>> def f(x, y=1, z):
AssertionError: Failed example:
    def f(x, y=1, z):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: parameter without a default follows parameter with a default
Got:
      File "<doctest __main__[90]>", line 1
        def f(x, y=1, z):
                       ^^
    SyntaxError: non-default argument follows default argument

======================================================================
FAIL: __main__ () [91]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 436, in __main__
    >>> def f(x, /, y=1, z):
AssertionError: Failed example:
    def f(x, /, y=1, z):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: parameter without a default follows parameter with a default
Got:
      File "<doctest __main__[91]>", line 1
        def f(x, /, y=1, z):
                          ^^
    SyntaxError: non-default argument follows default argument

======================================================================
FAIL: __main__ () [92]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 441, in __main__
    >>> def f(x, None):
AssertionError: Failed example:
    def f(x, None):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[92]>", line 1
        def f(x, None):
                 ^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [93]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 446, in __main__
    >>> def f(*None):
AssertionError: Failed example:
    def f(*None):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[93]>", line 1
        def f(*None):
               ^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [94]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 451, in __main__
    >>> def f(**None):
AssertionError: Failed example:
    def f(**None):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[94]>", line 1
        def f(**None):
                ^^^^^^
    SyntaxError: expected a name after '**'

======================================================================
FAIL: __main__ () [95]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 456, in __main__
    >>> def foo(/,a,b=,c):
AssertionError: Failed example:
    def foo(/,a,b=,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: at least one parameter must precede /
Got:
      File "<doctest __main__[95]>", line 1
        def foo(/,a,b=,c):
                      ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [96]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 461, in __main__
    >>> def foo(a,/,/,b,c):
AssertionError: Failed example:
    def foo(a,/,/,b,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got nothing

======================================================================
FAIL: __main__ () [97]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 466, in __main__
    >>> def foo(a,/,a1,/,b,c):
AssertionError: Failed example:
    def foo(a,/,a1,/,b,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got nothing

======================================================================
FAIL: __main__ () [98]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 471, in __main__
    >>> def foo(a=1,/,/,*b,/,c):
AssertionError: Failed example:
    def foo(a=1,/,/,*b,/,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got nothing

======================================================================
FAIL: __main__ () [99]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 476, in __main__
    >>> def foo(a,/,a1=1,/,b,c):
AssertionError: Failed example:
    def foo(a,/,a1=1,/,b,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got:
      File "<doctest __main__[99]>", line 1
        def foo(a,/,a1=1,/,b,c):
                            ^^^^
    SyntaxError: non-default argument follows default argument

======================================================================
FAIL: __main__ () [100]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 481, in __main__
    >>> def foo(a,*b,c,/,d,e):
AssertionError: Failed example:
    def foo(a,*b,c,/,d,e):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: / must be ahead of *
Got nothing

======================================================================
FAIL: __main__ () [101]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 486, in __main__
    >>> def foo(a=1,*b,c=3,/,d,e):
AssertionError: Failed example:
    def foo(a=1,*b,c=3,/,d,e):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: / must be ahead of *
Got nothing

======================================================================
FAIL: __main__ () [102]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 491, in __main__
    >>> def foo(a,*b=3,c):
AssertionError: Failed example:
    def foo(a,*b=3,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: var-positional parameter cannot have default value
Got:
      File "<doctest __main__[102]>", line 1
        def foo(a,*b=3,c):
                    ^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [103]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 496, in __main__
    >>> def foo(a,*b: int=,c):
AssertionError: Failed example:
    def foo(a,*b: int=,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: var-positional parameter cannot have default value
Got:
      File "<doctest __main__[103]>", line 1
        def foo(a,*b: int=,c):
                         ^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [104]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 501, in __main__
    >>> def foo(a,**b=3):
AssertionError: Failed example:
    def foo(a,**b=3):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: var-keyword parameter cannot have default value
Got:
      File "<doctest __main__[104]>", line 1
        def foo(a,**b=3):
                     ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [105]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 506, in __main__
    >>> def foo(a,**b: int=3):
AssertionError: Failed example:
    def foo(a,**b: int=3):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: var-keyword parameter cannot have default value
Got:
      File "<doctest __main__[105]>", line 1
        def foo(a,**b: int=3):
                          ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [106]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 511, in __main__
    >>> def foo(a,*a, b, **c, d):
AssertionError: Failed example:
    def foo(a,*a, b, **c, d):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[106]>", line 1
        def foo(a,*a, b, **c, d):
                              ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [107]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 516, in __main__
    >>> def foo(a,*a, b, **c, d=4):
AssertionError: Failed example:
    def foo(a,*a, b, **c, d=4):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[107]>", line 1
        def foo(a,*a, b, **c, d=4):
                              ^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [108]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 521, in __main__
    >>> def foo(a,*a, b, **c, *d):
AssertionError: Failed example:
    def foo(a,*a, b, **c, *d):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[108]>", line 1
        def foo(a,*a, b, **c, *d):
                              ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [109]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 526, in __main__
    >>> def foo(a,*a, b, **c, **d):
AssertionError: Failed example:
    def foo(a,*a, b, **c, **d):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[109]>", line 1
        def foo(a,*a, b, **c, **d):
                              ^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [110]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 531, in __main__
    >>> def foo(a=1,/,**b,/,c):
AssertionError: Failed example:
    def foo(a=1,/,**b,/,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[110]>", line 1
        def foo(a=1,/,**b,/,c):
                          ^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [111]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 536, in __main__
    >>> def foo(*b,*d):
AssertionError: Failed example:
    def foo(*b,*d):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got nothing

======================================================================
FAIL: __main__ () [112]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 541, in __main__
    >>> def foo(a,*b,c,*d,*e,c):
AssertionError: Failed example:
    def foo(a,*b,c,*d,*e,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got:
      File "<doctest __main__[112]>", line 1
        def foo(a,*b,c,*d,*e,c):
                     ^^^^^^^^^^^
    SyntaxError: this expression is not compiled yet

======================================================================
FAIL: __main__ () [113]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 546, in __main__
    >>> def foo(a,b,/,c,*b,c,*d,*e,c):
AssertionError: Failed example:
    def foo(a,b,/,c,*b,c,*d,*e,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got:
      File "<doctest __main__[113]>", line 1
        def foo(a,b,/,c,*b,c,*d,*e,c):
            ^^^^^^^^^^^^^^^^^^^^^^^^^^
    SyntaxError: duplicate parameter 'b' in function definition

======================================================================
FAIL: __main__ () [114]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 551, in __main__
    >>> def foo(a,b,/,c,*b,c,*d,**e):
AssertionError: Failed example:
    def foo(a,b,/,c,*b,c,*d,**e):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got:
      File "<doctest __main__[114]>", line 1
        def foo(a,b,/,c,*b,c,*d,**e):
            ^^^^^^^^^^^^^^^^^^^^^^^^^
    SyntaxError: duplicate parameter 'b' in function definition

======================================================================
FAIL: __main__ () [115]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 556, in __main__
    >>> def foo(a=1,/*,b,c):
AssertionError: Failed example:
    def foo(a=1,/*,b,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected comma between / and *
Got:
      File "<doctest __main__[115]>", line 1
        def foo(a=1,/*,b,c):
                     ^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [116]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 561, in __main__
    >>> def foo(a=1,d=,c):
AssertionError: Failed example:
    def foo(a=1,d=,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected default value expression
Got:
      File "<doctest __main__[116]>", line 1
        def foo(a=1,d=,c):
                      ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [117]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 566, in __main__
    >>> def foo(a,d=,c):
AssertionError: Failed example:
    def foo(a,d=,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected default value expression
Got:
      File "<doctest __main__[117]>", line 1
        def foo(a,d=,c):
                    ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [118]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 571, in __main__
    >>> def foo(a,d: int=,c):
AssertionError: Failed example:
    def foo(a,d: int=,c):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected default value expression
Got:
      File "<doctest __main__[118]>", line 1
        def foo(a,d: int=,c):
                         ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [119]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 576, in __main__
    >>> lambda /,a,b,c: None
AssertionError: Failed example:
    lambda /,a,b,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: at least one parameter must precede /
Got:
    <function <lambda>>

======================================================================
FAIL: __main__ () [120]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 580, in __main__
    >>> lambda a,/,/,b,c: None
AssertionError: Failed example:
    lambda a,/,/,b,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got:
    <function <lambda>>

======================================================================
FAIL: __main__ () [121]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 584, in __main__
    >>> lambda a,/,a1,/,b,c: None
AssertionError: Failed example:
    lambda a,/,a1,/,b,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got:
    <function <lambda>>

======================================================================
FAIL: __main__ () [122]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 588, in __main__
    >>> lambda a=1,/,/,*b,/,c: None
AssertionError: Failed example:
    lambda a=1,/,/,*b,/,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got:
    <function <lambda>>

======================================================================
FAIL: __main__ () [123]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 592, in __main__
    >>> lambda a,/,a1=1,/,b,c: None
AssertionError: Failed example:
    lambda a,/,a1=1,/,b,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: / may appear only once
Got:
      File "<doctest __main__[123]>", line 1
        lambda a,/,a1=1,/,b,c: None
                           ^^^^^^^^
    SyntaxError: non-default argument follows default argument

======================================================================
FAIL: __main__ () [124]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 596, in __main__
    >>> lambda a,*b,c,/,d,e: None
AssertionError: Failed example:
    lambda a,*b,c,/,d,e: None
Expected:
    Traceback (most recent call last):
    SyntaxError: / must be ahead of *
Got:
    <function <lambda>>

======================================================================
FAIL: __main__ () [125]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 600, in __main__
    >>> lambda a=1,*b,c=3,/,d,e: None
AssertionError: Failed example:
    lambda a=1,*b,c=3,/,d,e: None
Expected:
    Traceback (most recent call last):
    SyntaxError: / must be ahead of *
Got:
    <function <lambda>>

======================================================================
FAIL: __main__ () [126]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 604, in __main__
    >>> lambda a=1,/*,b,c: None
AssertionError: Failed example:
    lambda a=1,/*,b,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: expected comma between / and *
Got:
      File "<doctest __main__[126]>", line 1
        lambda a=1,/*,b,c: None
                    ^^^^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [127]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 608, in __main__
    >>> lambda a,*b=3,c: None
AssertionError: Failed example:
    lambda a,*b=3,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: var-positional parameter cannot have default value
Got:
      File "<doctest __main__[127]>", line 1
        lambda a,*b=3,c: None
                   ^^^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [128]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 612, in __main__
    >>> lambda a,**b=3: None
AssertionError: Failed example:
    lambda a,**b=3: None
Expected:
    Traceback (most recent call last):
    SyntaxError: var-keyword parameter cannot have default value
Got:
      File "<doctest __main__[128]>", line 1
        lambda a,**b=3: None
                    ^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [129]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 616, in __main__
    >>> lambda a, *a, b, **c, d: None
AssertionError: Failed example:
    lambda a, *a, b, **c, d: None
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[129]>", line 1
        lambda a, *a, b, **c, d: None
                              ^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [130]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 620, in __main__
    >>> lambda a,*a, b, **c, d=4: None
AssertionError: Failed example:
    lambda a,*a, b, **c, d=4: None
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[130]>", line 1
        lambda a,*a, b, **c, d=4: None
                             ^^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [131]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 624, in __main__
    >>> lambda a,*a, b, **c, *d: None
AssertionError: Failed example:
    lambda a,*a, b, **c, *d: None
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[131]>", line 1
        lambda a,*a, b, **c, *d: None
                             ^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [132]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 628, in __main__
    >>> lambda a,*a, b, **c, **d: None
AssertionError: Failed example:
    lambda a,*a, b, **c, **d: None
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[132]>", line 1
        lambda a,*a, b, **c, **d: None
                             ^^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [133]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 632, in __main__
    >>> lambda a=1,/,**b,/,c: None
AssertionError: Failed example:
    lambda a=1,/,**b,/,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: parameters cannot follow var-keyword parameter
Got:
      File "<doctest __main__[133]>", line 1
        lambda a=1,/,**b,/,c: None
                         ^^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [134]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 636, in __main__
    >>> lambda *b,*d: None
AssertionError: Failed example:
    lambda *b,*d: None
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got:
    <function <lambda>>

======================================================================
FAIL: __main__ () [135]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 640, in __main__
    >>> lambda a,*b,c,*d,*e,c: None
AssertionError: Failed example:
    lambda a,*b,c,*d,*e,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got:
      File "<doctest __main__[135]>", line 1
        lambda a,*b,c,*d,*e,c: None
                    ^^^^^^^^^^^^^^^
    SyntaxError: this expression is not compiled yet

======================================================================
FAIL: __main__ () [136]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 644, in __main__
    >>> lambda a,b,/,c,*b,c,*d,*e,c: None
AssertionError: Failed example:
    lambda a,b,/,c,*b,c,*d,*e,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got:
      File "<doctest __main__[136]>", line 1
        lambda a,b,/,c,*b,c,*d,*e,c: None
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    SyntaxError: duplicate parameter 'b' in function definition

======================================================================
FAIL: __main__ () [137]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 648, in __main__
    >>> lambda a,b,/,c,*b,c,*d,**e: None
AssertionError: Failed example:
    lambda a,b,/,c,*b,c,*d,**e: None
Expected:
    Traceback (most recent call last):
    SyntaxError: * may appear only once
Got:
      File "<doctest __main__[137]>", line 1
        lambda a,b,/,c,*b,c,*d,**e: None
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    SyntaxError: duplicate parameter 'b' in function definition

======================================================================
FAIL: __main__ () [138]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 652, in __main__
    >>> lambda a=1,d=,c: None
AssertionError: Failed example:
    lambda a=1,d=,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: expected default value expression
Got:
      File "<doctest __main__[138]>", line 1
        lambda a=1,d=,c: None
                     ^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [139]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 656, in __main__
    >>> lambda a,d=,c: None
AssertionError: Failed example:
    lambda a,d=,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: expected default value expression
Got:
      File "<doctest __main__[139]>", line 1
        lambda a,d=,c: None
                   ^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [140]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 660, in __main__
    >>> lambda a,d=3,c: None
AssertionError: Failed example:
    lambda a,d=3,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: parameter without a default follows parameter with a default
Got:
      File "<doctest __main__[140]>", line 1
        lambda a,d=3,c: None
                      ^^^^^^
    SyntaxError: non-default argument follows default argument

======================================================================
FAIL: __main__ () [141]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 664, in __main__
    >>> lambda a,/,d=3,c: None
AssertionError: Failed example:
    lambda a,/,d=3,c: None
Expected:
    Traceback (most recent call last):
    SyntaxError: parameter without a default follows parameter with a default
Got:
      File "<doctest __main__[141]>", line 1
        lambda a,/,d=3,c: None
                        ^^^^^^
    SyntaxError: non-default argument follows default argument

======================================================================
FAIL: __main__ () [142]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 668, in __main__
    >>> import ast; ast.parse('''
AssertionError: Failed example:
    import ast; ast.parse('''
    def f(
        *, # type: int
        a, # type: int
    ):
        pass
    ''', type_comments=True)
Expected:
    Traceback (most recent call last):
    SyntaxError: bare * has associated type comment
Got:
    Module(body=[FunctionDef(name='f', args=arguments(posonlyargs=[], args=[], vararg=None, kwonlyargs=[arg(...)], kw_defaults=[None], kwarg=None, defaults=[]), body=[Pass()], decorator_list=[], returns=None, type_comment=None, type_params=[])], type_ignores=[])

======================================================================
FAIL: __main__ () [143]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 681, in __main__
    >>> def None(x):
AssertionError: Failed example:
    def None(x):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[143]>", line 1
        def None(x):
            ^^^^^^^^
    SyntaxError: expected a function name

======================================================================
FAIL: __main__ () [147]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 694, in __main__
    >>> f(x for x in L, 1)
AssertionError: Failed example:
    f(x for x in L, 1)
Expected:
    Traceback (most recent call last):
    SyntaxError: Generator expression must be parenthesized
Got:
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

======================================================================
FAIL: __main__ () [148]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 697, in __main__
    >>> f(x for x in L, y=1)
AssertionError: Failed example:
    f(x for x in L, y=1)
Expected:
    Traceback (most recent call last):
    SyntaxError: Generator expression must be parenthesized
Got:
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

======================================================================
FAIL: __main__ () [149]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 700, in __main__
    >>> f(x for x in L, *[])
AssertionError: Failed example:
    f(x for x in L, *[])
Expected:
    Traceback (most recent call last):
    SyntaxError: Generator expression must be parenthesized
Got:
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

======================================================================
FAIL: __main__ () [150]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 703, in __main__
    >>> f(x for x in L, **{})
AssertionError: Failed example:
    f(x for x in L, **{})
Expected:
    Traceback (most recent call last):
    SyntaxError: Generator expression must be parenthesized
Got:
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

======================================================================
FAIL: __main__ () [151]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 706, in __main__
    >>> f(L, x for x in L)
AssertionError: Failed example:
    f(L, x for x in L)
Expected:
    Traceback (most recent call last):
    SyntaxError: Generator expression must be parenthesized
Got:
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

======================================================================
FAIL: __main__ () [152]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 709, in __main__
    >>> f(x for x in L, y for y in L)
AssertionError: Failed example:
    f(x for x in L, y for y in L)
Expected:
    Traceback (most recent call last):
    SyntaxError: Generator expression must be parenthesized
Got:
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

======================================================================
FAIL: __main__ () [153]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 712, in __main__
    >>> f(x for x in L,)
AssertionError: Failed example:
    f(x for x in L,)
Expected:
    Traceback (most recent call last):
    SyntaxError: Generator expression must be parenthesized
Got:
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

======================================================================
FAIL: __main__ () [155]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 717, in __main__
    >>> class C(x for x in L):
AssertionError: Failed example:
    class C(x for x in L):
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[155]>", line 1
        class C(x for x in L):
                  ^^^^^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [162]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 817, in __main__
    >>> f(lambda x: x[0] = 3)
AssertionError: Failed example:
    f(lambda x: x[0] = 3)
Expected:
    Traceback (most recent call last):
    SyntaxError: expression cannot contain assignment, perhaps you meant "=="?
Got:
      File "<doctest __main__[162]>", line 1
        f(lambda x: x[0] = 3)
                         ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [164]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 829, in __main__
    >>> f(x()=2)
AssertionError: Failed example:
    f(x()=2)
Expected:
    Traceback (most recent call last):
    SyntaxError: expression cannot contain assignment, perhaps you meant "=="?
Got:
      File "<doctest __main__[164]>", line 1
        f(x()=2)
             ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [165]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 832, in __main__
    >>> f(a or b=1)
AssertionError: Failed example:
    f(a or b=1)
Expected:
    Traceback (most recent call last):
    SyntaxError: expression cannot contain assignment, perhaps you meant "=="?
Got:
      File "<doctest __main__[165]>", line 1
        f(a or b=1)
                ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [166]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 835, in __main__
    >>> f(x.y=1)
AssertionError: Failed example:
    f(x.y=1)
Expected:
    Traceback (most recent call last):
    SyntaxError: expression cannot contain assignment, perhaps you meant "=="?
Got:
      File "<doctest __main__[166]>", line 1
        f(x.y=1)
             ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [167]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 838, in __main__
    >>> f((x)=2)
AssertionError: Failed example:
    f((x)=2)
Expected:
    Traceback (most recent call last):
    SyntaxError: expression cannot contain assignment, perhaps you meant "=="?
Got:
      File "<doctest __main__[167]>", line 1
        f((x)=2)
             ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [168]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 841, in __main__
    >>> f(True=1)
AssertionError: Failed example:
    f(True=1)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to True
Got:
      File "<doctest __main__[168]>", line 1
        f(True=1)
              ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [169]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 844, in __main__
    >>> f(False=1)
AssertionError: Failed example:
    f(False=1)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to False
Got:
      File "<doctest __main__[169]>", line 1
        f(False=1)
               ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [170]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 847, in __main__
    >>> f(None=1)
AssertionError: Failed example:
    f(None=1)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to None
Got:
      File "<doctest __main__[170]>", line 1
        f(None=1)
              ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [174]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 859, in __main__
    >>> f(a=)
AssertionError: Failed example:
    f(a=)
Expected:
    Traceback (most recent call last):
    SyntaxError: expected argument value expression
Got:
      File "<doctest __main__[174]>", line 1
        f(a=)
            ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [175]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 862, in __main__
    >>> f(a, b, c=)
AssertionError: Failed example:
    f(a, b, c=)
Expected:
    Traceback (most recent call last):
    SyntaxError: expected argument value expression
Got:
      File "<doctest __main__[175]>", line 1
        f(a, b, c=)
                  ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [176]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 865, in __main__
    >>> f(a, b, c=, d)
AssertionError: Failed example:
    f(a, b, c=, d)
Expected:
    Traceback (most recent call last):
    SyntaxError: expected argument value expression
Got:
      File "<doctest __main__[176]>", line 1
        f(a, b, c=, d)
                  ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [177]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 868, in __main__
    >>> f(*args=[0])
AssertionError: Failed example:
    f(*args=[0])
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to iterable argument unpacking
Got:
      File "<doctest __main__[177]>", line 1
        f(*args=[0])
               ^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [178]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 871, in __main__
    >>> f(a, b, *args=[0])
AssertionError: Failed example:
    f(a, b, *args=[0])
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to iterable argument unpacking
Got:
      File "<doctest __main__[178]>", line 1
        f(a, b, *args=[0])
                     ^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [179]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 874, in __main__
    >>> f(**kwargs={'a': 1})
AssertionError: Failed example:
    f(**kwargs={'a': 1})
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to keyword argument unpacking
Got:
      File "<doctest __main__[179]>", line 1
        f(**kwargs={'a': 1})
                  ^^^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [180]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 877, in __main__
    >>> f(a, b, *args, **kwargs={'a': 1})
AssertionError: Failed example:
    f(a, b, *args, **kwargs={'a': 1})
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to keyword argument unpacking
Got:
      File "<doctest __main__[180]>", line 1
        f(a, b, *args, **kwargs={'a': 1})
                               ^^^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [181]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 884, in __main__
    >>> (x for x in x) += 1
AssertionError: Failed example:
    (x for x in x) += 1
Expected:
    Traceback (most recent call last):
    SyntaxError: 'generator expression' is an illegal expression for augmented assignment
Got:
      File "<doctest __main__[181]>", line 1
        (x for x in x) += 1
        ^^^^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [182]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 887, in __main__
    >>> None += 1
AssertionError: Failed example:
    None += 1
Expected:
    Traceback (most recent call last):
    SyntaxError: 'None' is an illegal expression for augmented assignment
Got:
      File "<doctest __main__[182]>", line 1
        None += 1
        ^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [184]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 893, in __main__
    >>> f() += 1
AssertionError: Failed example:
    f() += 1
Expected:
    Traceback (most recent call last):
    SyntaxError: 'function call' is an illegal expression for augmented assignment
Got:
      File "<doctest __main__[184]>", line 1
        f() += 1
         ^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [197]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 990, in __main__
    >>> if a % 2 == 0:
AssertionError: Failed example:
    if a % 2 == 0:
        pass
    else:
        pass
    elif a % 2 == 1:
        pass
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: 'elif' block follows an 'else' block
Got:
      File "<doctest __main__[197]>", line 5
        elif a % 2 == 1:
        ^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [198]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1002, in __main__
    >>> def f():
AssertionError: Failed example:
    def f():
        print(x)
        global x
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: name 'x' is used prior to global declaration
Got nothing

======================================================================
FAIL: __main__ () [199]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1009, in __main__
    >>> def f():
AssertionError: Failed example:
    def f():
        x = 1
        global x
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: name 'x' is assigned to before global declaration
Got nothing

======================================================================
FAIL: __main__ () [200]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1016, in __main__
    >>> def f(x):
AssertionError: Failed example:
    def f(x):
        global x
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: name 'x' is parameter and global
Got:
      File "<doctest __main__[200]>", line 2
        global x
               ^
    SyntaxError: name is parameter and global

======================================================================
FAIL: __main__ () [201]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1022, in __main__
    >>> def f():
AssertionError: Failed example:
    def f():
        x = 1
        def g():
            print(x)
            nonlocal x
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: name 'x' is used prior to nonlocal declaration
Got nothing

======================================================================
FAIL: __main__ () [202]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1031, in __main__
    >>> def f():
AssertionError: Failed example:
    def f():
        x = 1
        def g():
            x = 2
            nonlocal x
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: name 'x' is assigned to before nonlocal declaration
Got nothing

======================================================================
FAIL: __main__ () [203]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1040, in __main__
    >>> def f(x):
AssertionError: Failed example:
    def f(x):
        nonlocal x
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: name 'x' is parameter and nonlocal
Got:
      File "<doctest __main__[203]>", line 2
        nonlocal x
                 ^
    SyntaxError: name is parameter and global

======================================================================
FAIL: __main__ () [204]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1046, in __main__
    >>> def f():
AssertionError: Failed example:
    def f():
        global x
        nonlocal x
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: name 'x' is nonlocal and global
Got:
      File "<doctest __main__[204]>", line 3
        nonlocal x
                 ^
    SyntaxError: name is nonlocal and global

======================================================================
FAIL: __main__ () [208]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1078, in __main__
    >>> if 1:
AssertionError: Failed example:
    if 1:
      x() = 1
    elif 1:
      pass
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: cannot assign to function call here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[208]>", line 2
        x() = 1
         ^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [209]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1086, in __main__
    >>> if 1:
AssertionError: Failed example:
    if 1:
      pass
    elif 1:
      x() = 1
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: cannot assign to function call here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[209]>", line 4
        x() = 1
         ^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [210]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1094, in __main__
    >>> if 1:
AssertionError: Failed example:
    if 1:
      x() = 1
    elif 1:
      pass
    else:
      pass
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: cannot assign to function call here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[210]>", line 2
        x() = 1
         ^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [211]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1104, in __main__
    >>> if 1:
AssertionError: Failed example:
    if 1:
      pass
    elif 1:
      x() = 1
    else:
      pass
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: cannot assign to function call here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[211]>", line 4
        x() = 1
         ^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [212]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1114, in __main__
    >>> if 1:
AssertionError: Failed example:
    if 1:
      pass
    elif 1:
      pass
    else:
      x() = 1
Expected:
    Traceback (most recent call last):
      ...
    SyntaxError: cannot assign to function call here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[212]>", line 6
        x() = 1
         ^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [218]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1151, in __main__
    >>> class R&D:
AssertionError: Failed example:
    class R&D:
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[218]>", line 1
        class R&D:
               ^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [223]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1188, in __main__
    >>> for x in range 10:
AssertionError: Failed example:
    for x in range 10:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[223]>", line 1
        for x in range 10:
                       ^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [229]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1218, in __main__
    >>> with (blech as something)
AssertionError: Failed example:
    with (blech as something)
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected ':'
Got:
      File "<doctest __main__[229]>", line 1
        with (blech as something)
                    ^^^^^^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [231]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1228, in __main__
    >>> with (blech, block as something)
AssertionError: Failed example:
    with (blech, block as something)
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected ':'
Got:
      File "<doctest __main__[231]>", line 1
        with (blech, block as something)
                           ^^^^^^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [232]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1233, in __main__
    >>> with (blech, block as something, bluch)
AssertionError: Failed example:
    with (blech, block as something, bluch)
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected ':'
Got:
      File "<doctest __main__[232]>", line 1
        with (blech, block as something, bluch)
                           ^^^^^^^^^^^^^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [233]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1238, in __main__
    >>> with block ad something:
AssertionError: Failed example:
    with block ad something:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'and'?
Got:
      File "<doctest __main__[233]>", line 1
        with block ad something:
                   ^^^^^^^^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [235]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1248, in __main__
    >>> try:
AssertionError: Failed example:
    try:
      pass
    except
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected ':'
Got:
      File "<doctest __main__[235]>", line 3
        except
              ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [236]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1255, in __main__
    >>> match x
AssertionError: Failed example:
    match x
      case list():
          pass
Expected:
    Traceback (most recent call last):
    SyntaxError: expected ':'
Got:
      File "<doctest __main__[236]>", line 1
        match x
              ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [237]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1261, in __main__
    >>> match x x:
AssertionError: Failed example:
    match x x:
      case list():
          pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[237]>", line 1
        match x x:
                ^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [242]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1291, in __main__
    >>> match x:
AssertionError: Failed example:
    match x:
      case Foo(a, __debug__=1, b=2):
          pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to __debug__
Got:
    Traceback (most recent call last):
      File "<doctest __main__[242]>", line 1, in <module>
        match x:
    NameError: name 'x' is not defined

======================================================================
FAIL: __main__ () [243]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1297, in __main__
    >>> if x = 3:
AssertionError: Failed example:
    if x = 3:
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[243]>", line 1
        if x = 3:
             ^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [244]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1302, in __main__
    >>> while x = 3:
AssertionError: Failed example:
    while x = 3:
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[244]>", line 1
        while x = 3:
                ^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [245]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1307, in __main__
    >>> if x.a = 3:
AssertionError: Failed example:
    if x.a = 3:
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to attribute here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[245]>", line 1
        if x.a = 3:
               ^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [246]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1312, in __main__
    >>> while x.a = 3:
AssertionError: Failed example:
    while x.a = 3:
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to attribute here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[246]>", line 1
        while x.a = 3:
                  ^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [253]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1346, in __main__
    >>> def f(x, (y, z), w):
AssertionError: Failed example:
    def f(x, (y, z), w):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: Function parameters cannot be parenthesized
Got:
      File "<doctest __main__[253]>", line 1
        def f(x, (y, z), w):
                 ^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [254]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1351, in __main__
    >>> def f((x, y, z, w)):
AssertionError: Failed example:
    def f((x, y, z, w)):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: Function parameters cannot be parenthesized
Got:
      File "<doctest __main__[254]>", line 1
        def f((x, y, z, w)):
              ^^^^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [255]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1356, in __main__
    >>> def f(x, (y, z, w)):
AssertionError: Failed example:
    def f(x, (y, z, w)):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: Function parameters cannot be parenthesized
Got:
      File "<doctest __main__[255]>", line 1
        def f(x, (y, z, w)):
                 ^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [256]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1361, in __main__
    >>> def f((x, y, z), w):
AssertionError: Failed example:
    def f((x, y, z), w):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: Function parameters cannot be parenthesized
Got:
      File "<doctest __main__[256]>", line 1
        def f((x, y, z), w):
              ^^^^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [257]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1366, in __main__
    >>> lambda x, (y, z), w: None
AssertionError: Failed example:
    lambda x, (y, z), w: None
Expected:
    Traceback (most recent call last):
    SyntaxError: Lambda expression parameters cannot be parenthesized
Got:
      File "<doctest __main__[257]>", line 1
        lambda x, (y, z), w: None
                  ^^^^^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [258]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1370, in __main__
    >>> lambda (x, y, z, w): None
AssertionError: Failed example:
    lambda (x, y, z, w): None
Expected:
    Traceback (most recent call last):
    SyntaxError: Lambda expression parameters cannot be parenthesized
Got:
      File "<doctest __main__[258]>", line 1
        lambda (x, y, z, w): None
               ^^^^^^^^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [259]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1374, in __main__
    >>> lambda x, (y, z, w): None
AssertionError: Failed example:
    lambda x, (y, z, w): None
Expected:
    Traceback (most recent call last):
    SyntaxError: Lambda expression parameters cannot be parenthesized
Got:
      File "<doctest __main__[259]>", line 1
        lambda x, (y, z, w): None
                  ^^^^^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [260]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1378, in __main__
    >>> lambda (x, y, z), w: None
AssertionError: Failed example:
    lambda (x, y, z), w: None
Expected:
    Traceback (most recent call last):
    SyntaxError: Lambda expression parameters cannot be parenthesized
Got:
      File "<doctest __main__[260]>", line 1
        lambda (x, y, z), w: None
               ^^^^^^^^^^^^^^^^^^
    SyntaxError: expected a parameter name

======================================================================
FAIL: __main__ () [267]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1443, in __main__
    >>> try:
AssertionError: Failed example:
    try:
       pass
    except TypeError as obj.attr:
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use except statement with attribute
Got:
      File "<doctest __main__[267]>", line 3
        except TypeError as obj.attr:
                               ^^^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [268]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1450, in __main__
    >>> try:
AssertionError: Failed example:
    try:
       pass
    except TypeError as obj[1]:
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use except statement with subscript
Got:
      File "<doctest __main__[268]>", line 3
        except TypeError as obj[1]:
                               ^^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [269]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1457, in __main__
    >>> try:
AssertionError: Failed example:
    try:
       pass
    except* TypeError as (obj, name):
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use except* statement with tuple
Got:
      File "<doctest __main__[269]>", line 3
        except* TypeError as (obj, name):
                             ^^^^^^^^^^^^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [270]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1464, in __main__
    >>> try:
AssertionError: Failed example:
    try:
       pass
    except* TypeError as 1:
       pass
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use except* statement with literal
Got:
      File "<doctest __main__[270]>", line 3
        except* TypeError as 1:
                             ^^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [271]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1473, in __main__
    >>> try: pass
AssertionError: Failed example:
    try: pass
    except TypeError as name: raise from None
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression between 'raise' and 'from'?
Got:
      File "<doctest __main__[271]>", line 2
        except TypeError as name: raise from None
                                        ^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [272]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1478, in __main__
    >>> try: pass
AssertionError: Failed example:
    try: pass
    except* TypeError as name: raise from None
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression between 'raise' and 'from'?
Got:
      File "<doctest __main__[272]>", line 2
        except* TypeError as name: raise from None
                                         ^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [273]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1483, in __main__
    >>> match 1:
AssertionError: Failed example:
    match 1:
        case 1 | 2 as abc: raise from None
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression between 'raise' and 'from'?
Got:
      File "<doctest __main__[273]>", line 2
        case 1 | 2 as abc: raise from None
                                 ^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [278]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1507, in __main__
    >>> {1:2, 3:4, 5}
AssertionError: Failed example:
    {1:2, 3:4, 5}
Expected:
    Traceback (most recent call last):
    SyntaxError: ':' expected after dictionary key
Got:
      File "<doctest __main__[278]>", line 1
        {1:2, 3:4, 5}
                    ^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [279]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1511, in __main__
    >>> {1:2, 3:4, 5:}
AssertionError: Failed example:
    {1:2, 3:4, 5:}
Expected:
    Traceback (most recent call last):
    SyntaxError: expression expected after dictionary key and ':'
Got:
      File "<doctest __main__[279]>", line 1
        {1:2, 3:4, 5:}
                     ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [280]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1515, in __main__
    >>> {1: *12+1, 23: 1}
AssertionError: Failed example:
    {1: *12+1, 23: 1}
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use a starred expression in a dictionary value
Got:
      File "<doctest __main__[280]>", line 1
        {1: *12+1, 23: 1}
            ^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [281]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1519, in __main__
    >>> {1: *12+1}
AssertionError: Failed example:
    {1: *12+1}
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use a starred expression in a dictionary value
Got:
      File "<doctest __main__[281]>", line 1
        {1: *12+1}
            ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [282]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1523, in __main__
    >>> {1: 23, 1: *12+1}
AssertionError: Failed example:
    {1: 23, 1: *12+1}
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use a starred expression in a dictionary value
Got:
      File "<doctest __main__[282]>", line 1
        {1: 23, 1: *12+1}
                   ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [283]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1527, in __main__
    >>> {1:}
AssertionError: Failed example:
    {1:}
Expected:
    Traceback (most recent call last):
    SyntaxError: expression expected after dictionary key and ':'
Got:
      File "<doctest __main__[283]>", line 1
        {1:}
           ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [287]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1549, in __main__
    >>> while condition:
AssertionError: Failed example:
    while condition:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'while' statement on line 1
Got:
      File "<doctest __main__[287]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [288]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1554, in __main__
    >>> for x in range(10):
AssertionError: Failed example:
    for x in range(10):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'for' statement on line 1
Got:
      File "<doctest __main__[288]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [289]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1559, in __main__
    >>> for x in range(10):
AssertionError: Failed example:
    for x in range(10):
        pass
    else:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'else' statement on line 3
Got:
      File "<doctest __main__[289]>", line 4
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [290]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1566, in __main__
    >>> async for x in range(10):
AssertionError: Failed example:
    async for x in range(10):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'for' statement on line 1
Got:
      File "<doctest __main__[290]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [291]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1571, in __main__
    >>> async for x in range(10):
AssertionError: Failed example:
    async for x in range(10):
        pass
    else:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'else' statement on line 3
Got:
      File "<doctest __main__[291]>", line 4
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [292]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1578, in __main__
    >>> if something:
AssertionError: Failed example:
    if something:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'if' statement on line 1
Got:
      File "<doctest __main__[292]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [293]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1583, in __main__
    >>> if something:
AssertionError: Failed example:
    if something:
        pass
    elif something_else:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'elif' statement on line 3
Got:
      File "<doctest __main__[293]>", line 4
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [294]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1590, in __main__
    >>> if something:
AssertionError: Failed example:
    if something:
        pass
    elif something_else:
        pass
    else:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'else' statement on line 5
Got:
      File "<doctest __main__[294]>", line 6
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [295]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1599, in __main__
    >>> try:
AssertionError: Failed example:
    try:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'try' statement on line 1
Got:
      File "<doctest __main__[295]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [296]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1604, in __main__
    >>> try:
AssertionError: Failed example:
    try:
        something()
    except:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'except' statement on line 3
Got:
      File "<doctest __main__[296]>", line 4
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [297]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1611, in __main__
    >>> try:
AssertionError: Failed example:
    try:
        something()
    except A:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'except' statement on line 3
Got:
      File "<doctest __main__[297]>", line 4
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [298]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1618, in __main__
    >>> try:
AssertionError: Failed example:
    try:
        something()
    except* A:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'except*' statement on line 3
Got:
      File "<doctest __main__[298]>", line 4
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [299]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1625, in __main__
    >>> try:
AssertionError: Failed example:
    try:
        something()
    except A:
        pass
    finally:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'finally' statement on line 5
Got:
      File "<doctest __main__[299]>", line 6
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [300]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1634, in __main__
    >>> try:
AssertionError: Failed example:
    try:
        something()
    except* A:
        pass
    finally:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'finally' statement on line 5
Got:
      File "<doctest __main__[300]>", line 6
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [301]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1643, in __main__
    >>> with A:
AssertionError: Failed example:
    with A:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'with' statement on line 1
Got:
      File "<doctest __main__[301]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [302]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1648, in __main__
    >>> with A as a, B as b:
AssertionError: Failed example:
    with A as a, B as b:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'with' statement on line 1
Got:
      File "<doctest __main__[302]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [303]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1653, in __main__
    >>> with (A as a, B as b):
AssertionError: Failed example:
    with (A as a, B as b):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'with' statement on line 1
Got:
      File "<doctest __main__[303]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [304]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1658, in __main__
    >>> async with A:
AssertionError: Failed example:
    async with A:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'with' statement on line 1
Got:
      File "<doctest __main__[304]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [305]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1663, in __main__
    >>> async with A as a, B as b:
AssertionError: Failed example:
    async with A as a, B as b:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'with' statement on line 1
Got:
      File "<doctest __main__[305]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [306]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1668, in __main__
    >>> async with (A as a, B as b):
AssertionError: Failed example:
    async with (A as a, B as b):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'with' statement on line 1
Got:
      File "<doctest __main__[306]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [307]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1673, in __main__
    >>> def foo(x, /, y, *, z=2):
AssertionError: Failed example:
    def foo(x, /, y, *, z=2):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after function definition on line 1
Got:
      File "<doctest __main__[307]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [308]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1678, in __main__
    >>> def foo[T](x, /, y, *, z=2):
AssertionError: Failed example:
    def foo[T](x, /, y, *, z=2):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after function definition on line 1
Got:
      File "<doctest __main__[308]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [309]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1683, in __main__
    >>> class Blech(A):
AssertionError: Failed example:
    class Blech(A):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after class definition on line 1
Got:
      File "<doctest __main__[309]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [310]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1688, in __main__
    >>> class Blech[T](A):
AssertionError: Failed example:
    class Blech[T](A):
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after class definition on line 1
Got:
      File "<doctest __main__[310]>", line 2
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [314]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1706, in __main__
    >>> match something:
AssertionError: Failed example:
    match something:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'match' statement on line 1
Got:
      File "<doctest __main__[314]>", line 1
        match something:
              ^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [315]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1711, in __main__
    >>> match something:
AssertionError: Failed example:
    match something:
        case []:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'case' statement on line 2
Got:
      File "<doctest __main__[315]>", line 3
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [316]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1717, in __main__
    >>> match something:
AssertionError: Failed example:
    match something:
        case []:
            ...
        case {}:
    pass
Expected:
    Traceback (most recent call last):
    IndentationError: expected an indented block after 'case' statement on line 4
Got:
      File "<doctest __main__[316]>", line 5
        pass
        ^^^^
    IndentationError: expected an indented block

======================================================================
FAIL: __main__ () [319]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1737, in __main__
    >>> raise ValueError from
AssertionError: Failed example:
    raise ValueError from
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression after 'from'?
Got:
      File "<doctest __main__[319]>", line 1
        raise ValueError from
                             ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [320]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1741, in __main__
    >>> raise mod.ValueError() from
AssertionError: Failed example:
    raise mod.ValueError() from
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression after 'from'?
Got:
      File "<doctest __main__[320]>", line 1
        raise mod.ValueError() from
                                   ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [321]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1745, in __main__
    >>> raise from exc
AssertionError: Failed example:
    raise from exc
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression between 'raise' and 'from'?
Got:
      File "<doctest __main__[321]>", line 1
        raise from exc
              ^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [322]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1749, in __main__
    >>> raise from None
AssertionError: Failed example:
    raise from None
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression between 'raise' and 'from'?
Got:
      File "<doctest __main__[322]>", line 1
        raise from None
              ^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [323]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1753, in __main__
    >>> raise from
AssertionError: Failed example:
    raise from
Expected:
    Traceback (most recent call last):
    SyntaxError: did you forget an expression between 'raise' and 'from'?
Got:
      File "<doctest __main__[323]>", line 1
        raise from
              ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [329]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1806, in __main__
    >>> fur a in b:
AssertionError: Failed example:
    fur a in b:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'for'?
Got:
      File "<doctest __main__[329]>", line 1
        fur a in b:
            ^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [330]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1811, in __main__
    >>> for a in b:
AssertionError: Failed example:
    for a in b:
      pass
    elso:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'else'?
Got:
      File "<doctest __main__[330]>", line 3
        elso:
             ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [331]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1818, in __main__
    >>> whille True:
AssertionError: Failed example:
    whille True:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'while'?
Got:
      File "<doctest __main__[331]>", line 1
        whille True:
               ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [332]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1823, in __main__
    >>> while True:
AssertionError: Failed example:
    while True:
      pass
    elso:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'else'?
Got:
      File "<doctest __main__[332]>", line 3
        elso:
             ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [333]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1830, in __main__
    >>> iff x > 5:
AssertionError: Failed example:
    iff x > 5:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'if'?
Got:
      File "<doctest __main__[333]>", line 1
        iff x > 5:
            ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [334]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1835, in __main__
    >>> if x:
AssertionError: Failed example:
    if x:
      pass
    elseif y:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'elif'?
Got:
      File "<doctest __main__[334]>", line 3
        elseif y:
               ^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [335]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1842, in __main__
    >>> if x:
AssertionError: Failed example:
    if x:
      pass
    elif y:
      pass
    elso:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'else'?
Got:
      File "<doctest __main__[335]>", line 5
        elso:
             ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [336]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1851, in __main__
    >>> tyo:
AssertionError: Failed example:
    tyo:
      pass
    except y:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'try'?
Got:
      File "<doctest __main__[336]>", line 1
        tyo:
            ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [337]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1858, in __main__
    >>> classe MyClass:
AssertionError: Failed example:
    classe MyClass:
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'class'?
Got:
      File "<doctest __main__[337]>", line 1
        classe MyClass:
               ^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [338]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1863, in __main__
    >>> impor math
AssertionError: Failed example:
    impor math
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'import'?
Got:
      File "<doctest __main__[338]>", line 1
        impor math
              ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [339]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1867, in __main__
    >>> form x import y
AssertionError: Failed example:
    form x import y
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'from'?
Got:
      File "<doctest __main__[339]>", line 1
        form x import y
             ^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [340]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1871, in __main__
    >>> defn calculate_sum(a, b):
AssertionError: Failed example:
    defn calculate_sum(a, b):
      return a + b
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'def'?
Got:
      File "<doctest __main__[340]>", line 1
        defn calculate_sum(a, b):
             ^^^^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [341]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1876, in __main__
    >>> def foo():
AssertionError: Failed example:
    def foo():
      returm result
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'return'?
Got:
      File "<doctest __main__[341]>", line 2
        returm result
               ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [342]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1881, in __main__
    >>> lamda x: x ** 2
AssertionError: Failed example:
    lamda x: x ** 2
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'lambda'?
Got:
      File "<doctest __main__[342]>", line 1
        lamda x: x ** 2
              ^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [343]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1885, in __main__
    >>> def foo():
AssertionError: Failed example:
    def foo():
      yeld i
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'yield'?
Got:
      File "<doctest __main__[343]>", line 2
        yeld i
             ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [344]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1890, in __main__
    >>> def foo():
AssertionError: Failed example:
    def foo():
      globel counter
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'global'?
Got:
      File "<doctest __main__[344]>", line 2
        globel counter
               ^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [345]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1895, in __main__
    >>> frum math import sqrt
AssertionError: Failed example:
    frum math import sqrt
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'from'?
Got:
      File "<doctest __main__[345]>", line 1
        frum math import sqrt
             ^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [346]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1899, in __main__
    >>> asynch def fetch_data():
AssertionError: Failed example:
    asynch def fetch_data():
      pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'async'?
Got:
      File "<doctest __main__[346]>", line 1
        asynch def fetch_data():
               ^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [347]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1904, in __main__
    >>> async def foo():
AssertionError: Failed example:
    async def foo():
      awaid fetch_data()
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'await'?
Got:
      File "<doctest __main__[347]>", line 2
        awaid fetch_data()
              ^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [348]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1909, in __main__
    >>> raisee ValueError("Error")
AssertionError: Failed example:
    raisee ValueError("Error")
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'raise'?
Got:
      File "<doctest __main__[348]>", line 1
        raisee ValueError("Error")
               ^^^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [349]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1913, in __main__
    >>> [
AssertionError: Failed example:
    [
    x for x
    in range(3)
    of x
    ]
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'if'?
Got:
      File "<doctest __main__[349]>", line 4
        of x
        ^^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [350]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1921, in __main__
    >>> [
AssertionError: Failed example:
    [
    123 fur x
    in range(3)
    if x
    ]
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'for'?
Got:
      File "<doctest __main__[350]>", line 2
        123 fur x
            ^^^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [351]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1930, in __main__
    >>> for x im n:
AssertionError: Failed example:
    for x im n:
        pass
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Did you mean 'in'?
Got:
      File "<doctest __main__[351]>", line 1
        for x im n:
              ^^^^^
    SyntaxError: expected 'in'

======================================================================
FAIL: __main__ () [352]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1935, in __main__
    >>> f(a=23, a=234)
AssertionError: Failed example:
    f(a=23, a=234)
Expected:
    Traceback (most recent call last):
       ...
    SyntaxError: keyword argument repeated: a
Got:
    Traceback (most recent call last):
      File "<doctest __main__[352]>", line 1, in <module>
        f(a=23, a=234)
    TypeError: f() got an unexpected keyword argument 'a'

======================================================================
FAIL: __main__ () [353]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1940, in __main__
    >>> {1, 2, 3} = 42
AssertionError: Failed example:
    {1, 2, 3} = 42
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to set display here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[353]>", line 1
        {1, 2, 3} = 42
        ^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [354]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1944, in __main__
    >>> {1: 2, 3: 4} = 42
AssertionError: Failed example:
    {1: 2, 3: 4} = 42
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to dict literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[354]>", line 1
        {1: 2, 3: 4} = 42
        ^^^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [355]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1948, in __main__
    >>> f'{x}' = 42
AssertionError: Failed example:
    f'{x}' = 42
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to f-string expression here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[355]>", line 1
        f'{x}' = 42
        ^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [356]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1952, in __main__
    >>> f'{x}-{y}' = 42
AssertionError: Failed example:
    f'{x}-{y}' = 42
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to f-string expression here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[356]>", line 1
        f'{x}-{y}' = 42
        ^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [357]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1956, in __main__
    >>> ub''
AssertionError: Failed example:
    ub''
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'b' prefixes are incompatible
Got:
    b''

======================================================================
FAIL: __main__ () [358]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1960, in __main__
    >>> bu"привет"
AssertionError: Failed example:
    bu"привет"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'b' prefixes are incompatible
Got:
      File "<doctest __main__[358]>", line 1
        bu"привет"
           ^^^^^^^
    SyntaxError: bytes can only contain ASCII literal characters

======================================================================
FAIL: __main__ () [359]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1964, in __main__
    >>> ur''
AssertionError: Failed example:
    ur''
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'r' prefixes are incompatible
Got:
    ''

======================================================================
FAIL: __main__ () [360]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1968, in __main__
    >>> ru"\t"
AssertionError: Failed example:
    ru" "
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'r' prefixes are incompatible
Got:
    ' '

======================================================================
FAIL: __main__ () [361]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1972, in __main__
    >>> uf'{1 + 1}'
AssertionError: Failed example:
    uf'{1 + 1}'
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'f' prefixes are incompatible
Got:
    '2'

======================================================================
FAIL: __main__ () [362]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1976, in __main__
    >>> fu""
AssertionError: Failed example:
    fu""
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'f' prefixes are incompatible
Got:
    ''

======================================================================
FAIL: __main__ () [363]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1980, in __main__
    >>> ut'{1}'
AssertionError: Failed example:
    ut'{1}'
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 't' prefixes are incompatible
Got:
    Template(strings=('', ''), interpolations=(Interpolation(1, '1', None, ''),))

======================================================================
FAIL: __main__ () [364]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1984, in __main__
    >>> tu"234"
AssertionError: Failed example:
    tu"234"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 't' prefixes are incompatible
Got:
    Template(strings=('234',), interpolations=())

======================================================================
FAIL: __main__ () [365]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1988, in __main__
    >>> bf'{x!r}'
AssertionError: Failed example:
    bf'{x!r}'
Expected:
    Traceback (most recent call last):
    SyntaxError: 'b' and 'f' prefixes are incompatible
Got:
      File "<doctest __main__[365]>", line 1
        bf'{x!r}'
          ^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [366]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1992, in __main__
    >>> fb"text"
AssertionError: Failed example:
    fb"text"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'b' and 'f' prefixes are incompatible
Got:
      File "<doctest __main__[366]>", line 1
        fb"text"
          ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [367]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 1996, in __main__
    >>> bt"text"
AssertionError: Failed example:
    bt"text"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'b' and 't' prefixes are incompatible
Got:
      File "<doctest __main__[367]>", line 1
        bt"text"
          ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [368]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2000, in __main__
    >>> tb''
AssertionError: Failed example:
    tb''
Expected:
    Traceback (most recent call last):
    SyntaxError: 'b' and 't' prefixes are incompatible
Got:
      File "<doctest __main__[368]>", line 1
        tb''
          ^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [369]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2004, in __main__
    >>> tf"{0.3:.02f}"
AssertionError: Failed example:
    tf"{0.3:.02f}"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'f' and 't' prefixes are incompatible
Got:
      File "<doctest __main__[369]>", line 1
        tf"{0.3:.02f}"
          ^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [370]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2008, in __main__
    >>> ft'{x=}'
AssertionError: Failed example:
    ft'{x=}'
Expected:
    Traceback (most recent call last):
    SyntaxError: 'f' and 't' prefixes are incompatible
Got:
      File "<doctest __main__[370]>", line 1
        ft'{x=}'
          ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [371]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2012, in __main__
    >>> tfu"{x=}"
AssertionError: Failed example:
    tfu"{x=}"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'f' prefixes are incompatible
Got:
      File "<doctest __main__[371]>", line 1
        tfu"{x=}"
           ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [372]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2016, in __main__
    >>> turf"{x=}"
AssertionError: Failed example:
    turf"{x=}"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'r' prefixes are incompatible
Got:
      File "<doctest __main__[372]>", line 1
        turf"{x=}"
            ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [373]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2020, in __main__
    >>> burft"{x=}"
AssertionError: Failed example:
    burft"{x=}"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'u' and 'b' prefixes are incompatible
Got:
      File "<doctest __main__[373]>", line 1
        burft"{x=}"
             ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [374]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2024, in __main__
    >>> brft"{x=}"
AssertionError: Failed example:
    brft"{x=}"
Expected:
    Traceback (most recent call last):
    SyntaxError: 'b' and 'f' prefixes are incompatible
Got:
      File "<doctest __main__[374]>", line 1
        brft"{x=}"
            ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [375]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2028, in __main__
    >>> t'{x}' = 42
AssertionError: Failed example:
    t'{x}' = 42
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to t-string expression here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[375]>", line 1
        t'{x}' = 42
        ^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [376]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2032, in __main__
    >>> t'{x}-{y}' = 42
AssertionError: Failed example:
    t'{x}-{y}' = 42
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to t-string expression here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[376]>", line 1
        t'{x}-{y}' = 42
        ^^^^^^^^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [377]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2036, in __main__
    >>> (x, y, z=3, d, e)
AssertionError: Failed example:
    (x, y, z=3, d, e)
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[377]>", line 1
        (x, y, z=3, d, e)
                ^^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [378]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2040, in __main__
    >>> [x, y, z=3, d, e]
AssertionError: Failed example:
    [x, y, z=3, d, e]
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[378]>", line 1
        [x, y, z=3, d, e]
                ^^^^^^^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [379]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2044, in __main__
    >>> [z=3]
AssertionError: Failed example:
    [z=3]
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[379]>", line 1
        [z=3]
          ^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [380]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2048, in __main__
    >>> {x, y, z=3, d, e}
AssertionError: Failed example:
    {x, y, z=3, d, e}
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[380]>", line 1
        {x, y, z=3, d, e}
                ^^^^^^^^^
    SyntaxError: expected '}'

======================================================================
FAIL: __main__ () [381]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2052, in __main__
    >>> {z=3}
AssertionError: Failed example:
    {z=3}
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[381]>", line 1
        {z=3}
          ^^^
    SyntaxError: expected '}'

======================================================================
FAIL: __main__ () [382]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2056, in __main__
    >>> from t import x,
AssertionError: Failed example:
    from t import x,
Expected:
    Traceback (most recent call last):
    SyntaxError: trailing comma not allowed without surrounding parentheses
Got:
      File "<doctest __main__[382]>", line 1
        from t import x,
                        ^
    SyntaxError: expected a name

======================================================================
FAIL: __main__ () [383]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2060, in __main__
    >>> from t import x,y,
AssertionError: Failed example:
    from t import x,y,
Expected:
    Traceback (most recent call last):
    SyntaxError: trailing comma not allowed without surrounding parentheses
Got:
      File "<doctest __main__[383]>", line 1
        from t import x,y,
                          ^
    SyntaxError: expected a name

======================================================================
FAIL: __main__ () [384]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2064, in __main__
    >>> with item,: pass
AssertionError: Failed example:
    with item,: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: the last 'with' item has a trailing comma
Got:
      File "<doctest __main__[384]>", line 1
        with item,: pass
                  ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [385]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2068, in __main__
    >>> with item as x,: pass
AssertionError: Failed example:
    with item as x,: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: the last 'with' item has a trailing comma
Got:
      File "<doctest __main__[385]>", line 1
        with item as x,: pass
                       ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [386]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2072, in __main__
    >>> with item1, item2,: pass
AssertionError: Failed example:
    with item1, item2,: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: the last 'with' item has a trailing comma
Got:
      File "<doctest __main__[386]>", line 1
        with item1, item2,: pass
                          ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [387]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2076, in __main__
    >>> with item1 as x, item2,: pass
AssertionError: Failed example:
    with item1 as x, item2,: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: the last 'with' item has a trailing comma
Got:
      File "<doctest __main__[387]>", line 1
        with item1 as x, item2,: pass
                               ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [388]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2080, in __main__
    >>> with item1 as x, item2 as y,: pass
AssertionError: Failed example:
    with item1 as x, item2 as y,: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: the last 'with' item has a trailing comma
Got:
      File "<doctest __main__[388]>", line 1
        with item1 as x, item2 as y,: pass
                                    ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [389]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2084, in __main__
    >>> with item1, item2 as y,: pass
AssertionError: Failed example:
    with item1, item2 as y,: pass
Expected:
    Traceback (most recent call last):
    SyntaxError: the last 'with' item has a trailing comma
Got:
      File "<doctest __main__[389]>", line 1
        with item1, item2 as y,: pass
                               ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [390]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2088, in __main__
    >>> import a from b
AssertionError: Failed example:
    import a from b
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[390]>", line 1
        import a from b
                 ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [391]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2092, in __main__
    >>> import a.y.z from b.y.z
AssertionError: Failed example:
    import a.y.z from b.y.z
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[391]>", line 1
        import a.y.z from b.y.z
                     ^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [392]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2096, in __main__
    >>> import a from b as bar
AssertionError: Failed example:
    import a from b as bar
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[392]>", line 1
        import a from b as bar
                 ^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [393]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2100, in __main__
    >>> import a.y.z from b.y.z as bar
AssertionError: Failed example:
    import a.y.z from b.y.z as bar
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[393]>", line 1
        import a.y.z from b.y.z as bar
                     ^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [394]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2104, in __main__
    >>> import a, b,c from b
AssertionError: Failed example:
    import a, b,c from b
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[394]>", line 1
        import a, b,c from b
                      ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [395]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2108, in __main__
    >>> import a.y.z, b.y.z, c.y.z from b.y.z
AssertionError: Failed example:
    import a.y.z, b.y.z, c.y.z from b.y.z
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[395]>", line 1
        import a.y.z, b.y.z, c.y.z from b.y.z
                                   ^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [396]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2112, in __main__
    >>> import a,b,c from b as bar
AssertionError: Failed example:
    import a,b,c from b as bar
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[396]>", line 1
        import a,b,c from b as bar
                     ^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [397]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2116, in __main__
    >>> import a.y.z, b.y.z, c.y.z from b.y.z as bar
AssertionError: Failed example:
    import a.y.z, b.y.z, c.y.z from b.y.z as bar
Expected:
    Traceback (most recent call last):
    SyntaxError: Did you mean to use 'from ... import ...' instead?
Got:
      File "<doctest __main__[397]>", line 1
        import a.y.z, b.y.z, c.y.z from b.y.z as bar
                                   ^^^^^^^^^^^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [403]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2140, in __main__
    >>> import a as b.c
AssertionError: Failed example:
    import a as b.c
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use attribute as import target
Got:
      File "<doctest __main__[403]>", line 1
        import a as b.c
                     ^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [404]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2144, in __main__
    >>> import a.b as (a, b)
AssertionError: Failed example:
    import a.b as (a, b)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use tuple as import target
Got:
      File "<doctest __main__[404]>", line 1
        import a.b as (a, b)
                      ^^^^^^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [405]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2148, in __main__
    >>> import a, a.b as 1
AssertionError: Failed example:
    import a, a.b as 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use literal as import target
Got:
      File "<doctest __main__[405]>", line 1
        import a, a.b as 1
                         ^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [406]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2152, in __main__
    >>> import a.b as 'a', a
AssertionError: Failed example:
    import a.b as 'a', a
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use literal as import target
Got:
      File "<doctest __main__[406]>", line 1
        import a.b as 'a', a
                      ^^^^^^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [407]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2156, in __main__
    >>> from a import (b as c.d)
AssertionError: Failed example:
    from a import (b as c.d)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use attribute as import target
Got:
      File "<doctest __main__[407]>", line 1
        from a import (b as c.d)
                             ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [408]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2160, in __main__
    >>> from a import b as 1
AssertionError: Failed example:
    from a import b as 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use literal as import target
Got:
      File "<doctest __main__[408]>", line 1
        from a import b as 1
                           ^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [409]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2164, in __main__
    >>> from a import (
AssertionError: Failed example:
    from a import (
      b as f())
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use function call as import target
Got:
      File "<doctest __main__[409]>", line 2
        b as f())
              ^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [410]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2169, in __main__
    >>> from a import (
AssertionError: Failed example:
    from a import (
      b as [],
    )
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use list as import target
Got:
      File "<doctest __main__[410]>", line 2
        b as [],
             ^^^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [411]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2175, in __main__
    >>> from a import (
AssertionError: Failed example:
    from a import (
      b,
      c as ()
    )
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use tuple as import target
Got:
      File "<doctest __main__[411]>", line 3
        c as ()
             ^^
    SyntaxError: expected a name after 'as'

======================================================================
FAIL: __main__ () [412]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2182, in __main__
    >>> from a import b, с as d[e]
AssertionError: Failed example:
    from a import b, с as d[e]
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use subscript as import target
Got:
      File "<doctest __main__[412]>", line 1
        from a import b, с as d[e]
                               ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [413]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2186, in __main__
    >>> from a import с as d[e], b
AssertionError: Failed example:
    from a import с as d[e], b
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use subscript as import target
Got:
      File "<doctest __main__[413]>", line 1
        from a import с as d[e], b
                            ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [414]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2193, in __main__
    >>> import a as b; None = 1
AssertionError: Failed example:
    import a as b; None = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to None
Got:
      File "<doctest __main__[414]>", line 1
        import a as b; None = 1
                       ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [415]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2197, in __main__
    >>> import a, b as c; d = 1; None = 1
AssertionError: Failed example:
    import a, b as c; d = 1; None = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to None
Got:
      File "<doctest __main__[415]>", line 1
        import a, b as c; d = 1; None = 1
                                 ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [416]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2201, in __main__
    >>> from a import b as c; None = 1
AssertionError: Failed example:
    from a import b as c; None = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to None
Got:
      File "<doctest __main__[416]>", line 1
        from a import b as c; None = 1
                              ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [417]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2205, in __main__
    >>> from a import b, c as d; e = 1; None = 1
AssertionError: Failed example:
    from a import b, c as d; e = 1; None = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to None
Got:
      File "<doctest __main__[417]>", line 1
        from a import b, c as d; e = 1; None = 1
                                        ^^^^^^^^
    SyntaxError: cannot assign to this

======================================================================
FAIL: __main__ () [418]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2212, in __main__
    >>> from t import x,y, and 3
AssertionError: Failed example:
    from t import x,y, and 3
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[418]>", line 1
        from t import x,y, and 3
                           ^^^^^
    SyntaxError: expected a name

======================================================================
FAIL: __main__ () [419]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2216, in __main__
    >>> from i import
AssertionError: Failed example:
    from i import
Expected:
    Traceback (most recent call last):
    SyntaxError: Expected one or more names after 'import'
Got:
      File "<doctest __main__[419]>", line 1
        from i import
                     ^
    SyntaxError: expected a name

======================================================================
FAIL: __main__ () [420]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2220, in __main__
    >>> from .. import
AssertionError: Failed example:
    from .. import
Expected:
    Traceback (most recent call last):
    SyntaxError: Expected one or more names after 'import'
Got:
      File "<doctest __main__[420]>", line 1
        from .. import
                      ^
    SyntaxError: expected a name

======================================================================
FAIL: __main__ () [421]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2224, in __main__
    >>> import
AssertionError: Failed example:
    import
Expected:
    Traceback (most recent call last):
    SyntaxError: Expected one or more names after 'import'
Got:
      File "<doctest __main__[421]>", line 1
        import
              ^
    SyntaxError: expected a module name

======================================================================
FAIL: __main__ () [422]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2228, in __main__
    >>> (): int
AssertionError: Failed example:
    (): int
Expected:
    Traceback (most recent call last):
    SyntaxError: only single target (not tuple) can be annotated
Got nothing

======================================================================
FAIL: __main__ () [423]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2231, in __main__
    >>> []: int
AssertionError: Failed example:
    []: int
Expected:
    Traceback (most recent call last):
    SyntaxError: only single target (not list) can be annotated
Got nothing

======================================================================
FAIL: __main__ () [424]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2234, in __main__
    >>> (()): int
AssertionError: Failed example:
    (()): int
Expected:
    Traceback (most recent call last):
    SyntaxError: only single target (not tuple) can be annotated
Got nothing

======================================================================
FAIL: __main__ () [425]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2237, in __main__
    >>> ([]): int
AssertionError: Failed example:
    ([]): int
Expected:
    Traceback (most recent call last):
    SyntaxError: only single target (not list) can be annotated
Got nothing

======================================================================
FAIL: __main__ () [426]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2241, in __main__
    >>> 1: int
AssertionError: Failed example:
    1: int
Expected:
    Traceback (most recent call last):
    SyntaxError: illegal target for annotation
Got:
      File "<doctest __main__[426]>", line 1
        1: int
        ^^^^^^
    SyntaxError: cannot annotate this

======================================================================
FAIL: __main__ () [427]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2244, in __main__
    >>> -x: int = 1
AssertionError: Failed example:
    -x: int = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: illegal target for annotation
Got:
      File "<doctest __main__[427]>", line 1
        -x: int = 1
        ^^^^^^^^^^^
    SyntaxError: cannot annotate this

======================================================================
FAIL: __main__ () [428]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2247, in __main__
    >>> (x for x in y): int
AssertionError: Failed example:
    (x for x in y): int
Expected:
    Traceback (most recent call last):
    SyntaxError: illegal target for annotation
Got:
      File "<doctest __main__[428]>", line 1
        (x for x in y): int
        ^^^^^^^^^^^^^^^^^^^
    SyntaxError: cannot annotate this

======================================================================
FAIL: __main__ () [429]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2253, in __main__
    >>> 3 + not 3
AssertionError: Failed example:
    3 + not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[429]>", line 1
        3 + not 3
            ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [430]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2257, in __main__
    >>> 3 * not 3
AssertionError: Failed example:
    3 * not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[430]>", line 1
        3 * not 3
            ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [431]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2261, in __main__
    >>> + not 3
AssertionError: Failed example:
    + not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[431]>", line 1
        + not 3
          ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [432]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2265, in __main__
    >>> - not 3
AssertionError: Failed example:
    - not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[432]>", line 1
        - not 3
          ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [433]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2269, in __main__
    >>> ~ not 3
AssertionError: Failed example:
    ~ not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[433]>", line 1
        ~ not 3
          ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [434]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2273, in __main__
    >>> 3 + - not 3
AssertionError: Failed example:
    3 + - not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[434]>", line 1
        3 + - not 3
              ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [435]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2277, in __main__
    >>> 3 + not -1
AssertionError: Failed example:
    3 + not -1
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[435]>", line 1
        3 + not -1
            ^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [436]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2281, in __main__
    >>> 1 << 2 + not 3
AssertionError: Failed example:
    1 << 2 + not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[436]>", line 1
        1 << 2 + not 3
                 ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [437]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2285, in __main__
    >>> 1 >> 2 * not 3
AssertionError: Failed example:
    1 >> 2 * not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[437]>", line 1
        1 >> 2 * not 3
                 ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [438]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2289, in __main__
    >>> 3 * + not 3
AssertionError: Failed example:
    3 * + not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[438]>", line 1
        3 * + not 3
              ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [439]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2293, in __main__
    >>> 3 ** - not 3
AssertionError: Failed example:
    3 ** - not 3
Expected:
    Traceback (most recent call last):
    SyntaxError: 'not' after an operator must be parenthesized
Got:
      File "<doctest __main__[439]>", line 1
        3 ** - not 3
               ^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [446]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2324, in __main__
    >>> with (lambda *:0): pass
AssertionError: Failed example:
    with (lambda *:0): pass
Expected:
    Traceback (most recent call last):
    SyntaxError: named parameters must follow bare *
Got:
    Traceback (most recent call last):
      File "<doctest __main__[446]>", line 1, in <module>
        with (lambda *:0): pass
    AttributeError: 'function' object has no attribute '__exit__'. Did you mean '.__init__' instead of '.__exit__'?

======================================================================
FAIL: __main__ () [451]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2350, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case 42 as 1+2+4:
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use expression as pattern target
Got:
      File "<doctest __main__[451]>", line 2
        case 42 as 1+2+4:
                   ^^^^^^
    SyntaxError: invalid pattern target

======================================================================
FAIL: __main__ () [452]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2356, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case 42 as a.b:
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use attribute as pattern target
Got:
      File "<doctest __main__[452]>", line 2
        case 42 as a.b:
                    ^^^
    SyntaxError: expected ':'

======================================================================
FAIL: __main__ () [453]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2362, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case 42 as (a, b):
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use tuple as pattern target
Got:
      File "<doctest __main__[453]>", line 2
        case 42 as (a, b):
                   ^^^^^^^
    SyntaxError: invalid pattern target

======================================================================
FAIL: __main__ () [454]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2368, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case 42 as (a + 1):
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use expression as pattern target
Got:
      File "<doctest __main__[454]>", line 2
        case 42 as (a + 1):
                   ^^^^^^^^
    SyntaxError: invalid pattern target

======================================================================
FAIL: __main__ () [455]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2374, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case (32 as x) | (42 as a()):
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use function call as pattern target
Got:
      File "<doctest __main__[455]>", line 2
        case (32 as x) | (42 as a()):
                                 ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [461]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2410, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case {**double_star, "spam": "eggs"}:
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: double star pattern must be the last (right-most) subpattern in the mapping pattern
Got:
      File "<doctest __main__[461]>", line 2
        case {**double_star, "spam": "eggs"}:
                             ^^^^^^^^^^^^^^^^
    SyntaxError: expected '}'

======================================================================
FAIL: __main__ () [462]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2416, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case {"foo": 1, **double_star, "spam": "eggs"}:
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: double star pattern must be the last (right-most) subpattern in the mapping pattern
Got:
      File "<doctest __main__[462]>", line 2
        case {"foo": 1, **double_star, "spam": "eggs"}:
                                       ^^^^^^^^^^^^^^^^
    SyntaxError: expected '}'

======================================================================
FAIL: __main__ () [463]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2422, in __main__
    >>> match ...:
AssertionError: Failed example:
    match ...:
      case {"spam": "eggs", "b": {**d, "ham": "bacon"}}:
        ...
Expected:
    Traceback (most recent call last):
    SyntaxError: double star pattern must be the last (right-most) subpattern in the mapping pattern
Got:
      File "<doctest __main__[463]>", line 2
        case {"spam": "eggs", "b": {**d, "ham": "bacon"}}:
                                         ^^^^^^^^^^^^^^^^^
    SyntaxError: expected '}'

======================================================================
FAIL: __main__ () [465]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2436, in __main__
    >>> A[:(*b)]
AssertionError: Failed example:
    A[:(*b)]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: cannot use starred expression here
Got:
      File "<doctest __main__[465]>", line 1
        A[:(*b)]
            ^^^^
    SyntaxError: can't use starred expression here

======================================================================
FAIL: __main__ () [468]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2451, in __main__
    >>> A[*b:]
AssertionError: Failed example:
    A[*b:]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[468]>", line 1
        A[*b:]
            ^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [469]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2455, in __main__
    >>> A[(*b):]
AssertionError: Failed example:
    A[(*b):]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: cannot use starred expression here
Got:
      File "<doctest __main__[469]>", line 1
        A[(*b):]
           ^^^^^
    SyntaxError: can't use starred expression here

======================================================================
FAIL: __main__ () [470]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2459, in __main__
    >>> A[*b:] = 1
AssertionError: Failed example:
    A[*b:] = 1
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[470]>", line 1
        A[*b:] = 1
            ^^^^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [471]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2463, in __main__
    >>> del A[*b:]
AssertionError: Failed example:
    del A[*b:]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[471]>", line 1
        del A[*b:]
                ^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [472]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2470, in __main__
    >>> A[*b:*b]
AssertionError: Failed example:
    A[*b:*b]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[472]>", line 1
        A[*b:*b]
            ^^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [473]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2474, in __main__
    >>> A[(*b:*b)]
AssertionError: Failed example:
    A[(*b:*b)]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[473]>", line 1
        A[(*b:*b)]
             ^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [474]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2478, in __main__
    >>> A[*b:*b] = 1
AssertionError: Failed example:
    A[*b:*b] = 1
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[474]>", line 1
        A[*b:*b] = 1
            ^^^^^^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [475]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2482, in __main__
    >>> del A[*b:*b]
AssertionError: Failed example:
    del A[*b:*b]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: invalid syntax
Got:
      File "<doctest __main__[475]>", line 1
        del A[*b:*b]
                ^^^^
    SyntaxError: expected ']'

======================================================================
FAIL: __main__ () [476]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2489, in __main__
    >>> A[*(1:2)]
AssertionError: Failed example:
    A[*(1:2)]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[476]>", line 1
        A[*(1:2)]
             ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [477]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2493, in __main__
    >>> A[*(1:2)] = 1
AssertionError: Failed example:
    A[*(1:2)] = 1
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[477]>", line 1
        A[*(1:2)] = 1
             ^^^^^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [478]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2497, in __main__
    >>> del A[*(1:2)]
AssertionError: Failed example:
    del A[*(1:2)]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[478]>", line 1
        del A[*(1:2)]
                 ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [479]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2504, in __main__
    >>> A[*:]
AssertionError: Failed example:
    A[*:]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[479]>", line 1
        A[*:]
           ^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [481]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2515, in __main__
    >>> A[*]
AssertionError: Failed example:
    A[*]
Expected:
    Traceback (most recent call last):
        ...
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[481]>", line 1
        A[*]
           ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [534]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2793, in __main__
    >>> f(**x, *)
AssertionError: Failed example:
    f(**x, *)
Expected:
    Traceback (most recent call last):
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[534]>", line 1
        f(**x, *)
               ^^
    SyntaxError: iterable argument unpacking follows keyword argument unpacking

======================================================================
FAIL: __main__ () [535]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2797, in __main__
    >>> f(x, *:)
AssertionError: Failed example:
    f(x, *:)
Expected:
    Traceback (most recent call last):
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[535]>", line 1
        f(x, *:)
              ^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [536]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2801, in __main__
    >>> f(x, *)
AssertionError: Failed example:
    f(x, *)
Expected:
    Traceback (most recent call last):
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[536]>", line 1
        f(x, *)
              ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [537]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2805, in __main__
    >>> f(x = 5, *)
AssertionError: Failed example:
    f(x = 5, *)
Expected:
    Traceback (most recent call last):
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[537]>", line 1
        f(x = 5, *)
                  ^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [538]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2809, in __main__
    >>> f(x = 5, *:)
AssertionError: Failed example:
    f(x = 5, *:)
Expected:
    Traceback (most recent call last):
    SyntaxError: Invalid star expression
Got:
      File "<doctest __main__[538]>", line 1
        f(x = 5, *:)
                  ^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [541]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2818, in __main__
    >>> assert a := 1
AssertionError: Failed example:
    assert a := 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use named expression without parentheses here
Got:
      File "<doctest __main__[541]>", line 1
        assert a := 1
                 ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [542]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2822, in __main__
    >>> assert 1, a := 1
AssertionError: Failed example:
    assert 1, a := 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot use named expression without parentheses here
Got:
      File "<doctest __main__[542]>", line 1
        assert 1, a := 1
                    ^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [543]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2826, in __main__
    >>> assert 1 = 2 = 3
AssertionError: Failed example:
    assert 1 = 2 = 3
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[543]>", line 1
        assert 1 = 2 = 3
                 ^^^^^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [544]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2830, in __main__
    >>> assert 1 = 2
AssertionError: Failed example:
    assert 1 = 2
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[544]>", line 1
        assert 1 = 2
                 ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [545]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2834, in __main__
    >>> assert (1 = 2)
AssertionError: Failed example:
    assert (1 = 2)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[545]>", line 1
        assert (1 = 2)
                  ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [546]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2838, in __main__
    >>> assert 'a' = a
AssertionError: Failed example:
    assert 'a' = a
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[546]>", line 1
        assert 'a' = a
                   ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [547]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2842, in __main__
    >>> assert x[0] = 1
AssertionError: Failed example:
    assert x[0] = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to subscript here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[547]>", line 1
        assert x[0] = 1
                    ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [548]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2846, in __main__
    >>> assert (yield a) = 2
AssertionError: Failed example:
    assert (yield a) = 2
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to yield expression here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[548]>", line 1
        assert (yield a) = 2
                         ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [549]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2850, in __main__
    >>> assert a = 2
AssertionError: Failed example:
    assert a = 2
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to name here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[549]>", line 1
        assert a = 2
                 ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [550]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2854, in __main__
    >>> assert (a = 2)
AssertionError: Failed example:
    assert (a = 2)
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[550]>", line 1
        assert (a = 2)
                  ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [551]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2858, in __main__
    >>> assert a = b
AssertionError: Failed example:
    assert a = b
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to name here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[551]>", line 1
        assert a = b
                 ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [552]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2862, in __main__
    >>> assert 1, 1 = b
AssertionError: Failed example:
    assert 1, 1 = b
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[552]>", line 1
        assert 1, 1 = b
                    ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [553]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2866, in __main__
    >>> assert 1, (1 = b)
AssertionError: Failed example:
    assert 1, (1 = b)
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[553]>", line 1
        assert 1, (1 = b)
                     ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [554]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2870, in __main__
    >>> assert 1, a = 1
AssertionError: Failed example:
    assert 1, a = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to name here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[554]>", line 1
        assert 1, a = 1
                    ^^^
    SyntaxError: invalid syntax

======================================================================
FAIL: __main__ () [555]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2874, in __main__
    >>> assert 1, (a = 1)
AssertionError: Failed example:
    assert 1, (a = 1)
Expected:
    Traceback (most recent call last):
    SyntaxError: invalid syntax. Maybe you meant '==' or ':=' instead of '='?
Got:
      File "<doctest __main__[555]>", line 1
        assert 1, (a = 1)
                     ^^^^
    SyntaxError: expected ')'

======================================================================
FAIL: __main__ () [556]
Doctest: __main__
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_syntax.py", line 2878, in __main__
    >>> assert 1 = a, a = 1
AssertionError: Failed example:
    assert 1 = a, a = 1
Expected:
    Traceback (most recent call last):
    SyntaxError: cannot assign to literal here. Maybe you meant '==' instead of '='?
Got:
      File "<doctest __main__[556]>", line 1
        assert 1 = a, a = 1
                 ^^^^^^^^^^
    SyntaxError: invalid syntax

----------------------------------------------------------------------
Ran 109 tests in Ns

FAILED (failures=427, skipped=16)
