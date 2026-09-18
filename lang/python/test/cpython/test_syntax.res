.Fs.......ssFFFsFFFFFFF.FFFFF..........F...FFFss.F..s.s..Fs.......ssFFFsFFFFFFF.FFFFF.....FFFss.F..s.s.FFF.F
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

----------------------------------------------------------------------
Ran 108 tests in Ns

FAILED (failures=45, skipped=16)
