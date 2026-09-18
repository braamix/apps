....................................................................................................................E.ssssssssssssssssssssssss.....................................................................................E......................................................................................................ssssssssssss....ssssssssssssssssssssssss.....ssssssssssssssssssssssssssssssssssssssssssssssss....................................................................................................................................................................................................................................................................................ssss......................................................................................................................................................................................................EEEEEEEEEEEE...................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................sssssssssssss.......................s..................FFFFFFFFFFFF............FFFFFFFFFFFF.
======================================================================
ERROR: test_pickle_roundtrip (__main__.TestArgumentParserPickleable.test_pickle_roundtrip)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 210, in test_pickle_roundtrip
    import pickle
ModuleNotFoundError: Standard library module 'pickle' was not found

======================================================================
ERROR: test_fake_color_theme_matches_real (__main__.TestColorized.test_fake_color_theme_matches_real)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 8055, in test_fake_color_theme_matches_real
    for k in _colorize_nocolor:
TypeError: '_NoColor' object is not iterable

======================================================================
ERROR: test_successes_many_groups_listargs (__main__.TestNegativeNumber.test_successes_many_groups_listargs) (args=['--complex', '-1_000j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 396, in listargs
    return parser.parse_args(args)
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000j'\n")

======================================================================
ERROR: test_successes_many_groups_listargs (__main__.TestNegativeNumber.test_successes_many_groups_listargs) (args=['--complex', '-1_000.0j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 396, in listargs
    return parser.parse_args(args)
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000.0j'\n")

======================================================================
ERROR: test_successes_many_groups_sysargs (__main__.TestNegativeNumber.test_successes_many_groups_sysargs) (args=['--complex', '-1_000j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 403, in sysargs
    return parser.parse_args()
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000j'\n")

======================================================================
ERROR: test_successes_many_groups_sysargs (__main__.TestNegativeNumber.test_successes_many_groups_sysargs) (args=['--complex', '-1_000.0j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 403, in sysargs
    return parser.parse_args()
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000.0j'\n")

======================================================================
ERROR: test_successes_no_groups_listargs (__main__.TestNegativeNumber.test_successes_no_groups_listargs) (args=['--complex', '-1_000j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 396, in listargs
    return parser.parse_args(args)
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000j'\n")

======================================================================
ERROR: test_successes_no_groups_listargs (__main__.TestNegativeNumber.test_successes_no_groups_listargs) (args=['--complex', '-1_000.0j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 396, in listargs
    return parser.parse_args(args)
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000.0j'\n")

======================================================================
ERROR: test_successes_no_groups_sysargs (__main__.TestNegativeNumber.test_successes_no_groups_sysargs) (args=['--complex', '-1_000j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 403, in sysargs
    return parser.parse_args()
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000j'\n")

======================================================================
ERROR: test_successes_no_groups_sysargs (__main__.TestNegativeNumber.test_successes_no_groups_sysargs) (args=['--complex', '-1_000.0j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 403, in sysargs
    return parser.parse_args()
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000.0j'\n")

======================================================================
ERROR: test_successes_one_group_listargs (__main__.TestNegativeNumber.test_successes_one_group_listargs) (args=['--complex', '-1_000j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 396, in listargs
    return parser.parse_args(args)
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000j'\n")

======================================================================
ERROR: test_successes_one_group_listargs (__main__.TestNegativeNumber.test_successes_one_group_listargs) (args=['--complex', '-1_000.0j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 396, in listargs
    return parser.parse_args(args)
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000.0j'\n")

======================================================================
ERROR: test_successes_one_group_sysargs (__main__.TestNegativeNumber.test_successes_one_group_sysargs) (args=['--complex', '-1_000j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 403, in sysargs
    return parser.parse_args()
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000j'\n")

======================================================================
ERROR: test_successes_one_group_sysargs (__main__.TestNegativeNumber.test_successes_one_group_sysargs) (args=['--complex', '-1_000.0j'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 451, in test_successes
    result_ns = self._parse_args(parser, args)
  File "/tmp/test_argparse.py", line 403, in sysargs
    return parser.parse_args()
  File "/tmp/test_argparse.py", line 334, in parse_args
    return stderr_to_parser_error(parse_args, *args, **kwargs)
  File "/tmp/test_argparse.py", line 323, in stderr_to_parser_error
    raise ArgumentParserError(
ArgumentParserError: ('SystemExit', '', "usage: test_argparse.py [-h] [--int INT] [--float FLOAT] [--complex COMPLEX]\ntest_argparse.py: error: argument --complex: invalid complex value: '-1_000.0j'\n")

======================================================================
FAIL: test_successes_many_groups_listargs (__main__.TestTypeClassicClass.test_successes_many_groups_listargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_many_groups_listargs (__main__.TestTypeClassicClass.test_successes_many_groups_listargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_many_groups_sysargs (__main__.TestTypeClassicClass.test_successes_many_groups_sysargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_many_groups_sysargs (__main__.TestTypeClassicClass.test_successes_many_groups_sysargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_no_groups_listargs (__main__.TestTypeClassicClass.test_successes_no_groups_listargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_no_groups_listargs (__main__.TestTypeClassicClass.test_successes_no_groups_listargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_no_groups_sysargs (__main__.TestTypeClassicClass.test_successes_no_groups_sysargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_no_groups_sysargs (__main__.TestTypeClassicClass.test_successes_no_groups_sysargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_one_group_listargs (__main__.TestTypeClassicClass.test_successes_one_group_listargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_one_group_listargs (__main__.TestTypeClassicClass.test_successes_one_group_listargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_one_group_sysargs (__main__.TestTypeClassicClass.test_successes_one_group_sysargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_one_group_sysargs (__main__.TestTypeClassicClass.test_successes_one_group_sysargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: NS(spam=<C object at 0xX>, x=<C object at 0xX>) != Namespace(x=<C object at 0xX>, spam=<C object at 0xX>)

======================================================================
FAIL: test_successes_many_groups_listargs (__main__.TestTypeUserDefined.test_successes_many_groups_listargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_many_groups_listargs (__main__.TestTypeUserDefined.test_successes_many_groups_listargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_many_groups_sysargs (__main__.TestTypeUserDefined.test_successes_many_groups_sysargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_many_groups_sysargs (__main__.TestTypeUserDefined.test_successes_many_groups_sysargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_no_groups_listargs (__main__.TestTypeUserDefined.test_successes_no_groups_listargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_no_groups_listargs (__main__.TestTypeUserDefined.test_successes_no_groups_listargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_no_groups_sysargs (__main__.TestTypeUserDefined.test_successes_no_groups_sysargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_no_groups_sysargs (__main__.TestTypeUserDefined.test_successes_no_groups_sysargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_one_group_listargs (__main__.TestTypeUserDefined.test_successes_one_group_listargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_one_group_listargs (__main__.TestTypeUserDefined.test_successes_one_group_listargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_one_group_sysargs (__main__.TestTypeUserDefined.test_successes_one_group_sysargs) (args=['a', '-x', 'b'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

======================================================================
FAIL: test_successes_one_group_sysargs (__main__.TestTypeUserDefined.test_successes_one_group_sysargs) (args=['-xf', 'g'])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 452, in test_successes
    tester.assertEqual(expected_ns, result_ns)
AssertionError: <NS object at 0xX> != <Namespace object at 0xX>

----------------------------------------------------------------------
Ran 1966 tests in Ns

FAILED (failures=24, errors=14, skipped=126)
