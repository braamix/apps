......................................................................................................................ssssssssssssssssssssssss.....................................................................................E......................................................................................................ssssssssssss....ssssssssssssssssssssssss.....ssssssssssssssssssssssssssssssssssssssssssssssss..............................................................................................................................................................................................................................................................................................................................................................................................................................................................................................EEEEEEEEEEEE...................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................FsssF.sFs.sss.......................s...........................................
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
FAIL: test_directory (__main__.TestProgName.test_directory)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 7364, in test_directory
    self.check_usage(f'{py} {dirname}', dirname)
  File "/tmp/test_argparse.py", line 7348, in check_usage
    res = script_helper.assert_python_ok('-Xutf8', *args, '-h', **kwargs)
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-I', '-Xutf8', 'packageæ/@test_N_tmpæ', '-h']

stdout:
---

---

stderr:
---
python: packageæ/@test_N_tmpæ: is a directory
---

======================================================================
FAIL: test_module (__main__.TestProgName.test_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 7375, in test_module
    self.check_usage(f'{py} -m {modulename}',
  File "/tmp/test_argparse.py", line 7348, in check_usage
    res = script_helper.assert_python_ok('-Xutf8', *args, '-h', **kwargs)
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-Xutf8', '-m', 'packageæ.moduleæ', '-h']

stdout:
---

---

stderr:
---
python: not a module name: packageæ.moduleæ
---

======================================================================
FAIL: test_package (__main__.TestProgName.test_package)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_argparse.py", line 7403, in test_package
    self.check_usage(f'{py} -m {packagename}',
  File "/tmp/test_argparse.py", line 7348, in check_usage
    res = script_helper.assert_python_ok('-Xutf8', *args, '-h', **kwargs)
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-Xutf8', '-m', 'packageæ.subpackageæ', '-h']

stdout:
---

---

stderr:
---
python: not a module name: packageæ.subpackageæ
---

----------------------------------------------------------------------
Ran 1966 tests in Ns

FAILED (failures=3, errors=13, skipped=117)
