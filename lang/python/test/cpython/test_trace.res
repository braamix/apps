EF..FFFsE.E.FEEEEE..EE......
======================================================================
ERROR: test_loop_caller_importing (__main__.TestCallers.test_loop_caller_importing)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 344, in test_loop_caller_importing
    self.tracer.runfunc(traced_func_importing_caller, 1)
  File "/pkg/store/python-0/lib/trace.py", line 467, in runfunc
    result = func(*args, **kw)
  File "/tmp/test_trace.py", line 63, in traced_func_importing_caller
    def traced_func_importing_caller(x):
  File "/pkg/store/python-0/lib/trace.py", line 521, in globaltrace_trackcallers
    this_func = self.file_module_function_of(frame)
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
    funcs = [f for f in gc.get_referrers(code)
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_coverage_ignore (__main__.TestCoverage.test_coverage_ignore)
----------------------------------------------------------------------
ModuleNotFoundError: No module named 'test.test_pprint'. Did you mean: 'test.test_email'?

======================================================================
ERROR: test_issue9936 (__main__.TestCoverage.test_issue9936)
----------------------------------------------------------------------
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_arg_errors (__main__.TestFuncs.test_arg_errors)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 289, in test_arg_errors
    res = self.tracer.runfunc(traced_capturer, 1, 2, self=3, func=4)
  File "/pkg/store/python-0/lib/trace.py", line 467, in runfunc
    result = func(*args, **kw)
  File "/tmp/test_trace.py", line 81, in traced_capturer
    def traced_capturer(*args, **kwargs):
  File "/pkg/store/python-0/lib/trace.py", line 531, in globaltrace_countfuncs
    this_func = self.file_module_function_of(frame)
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
    funcs = [f for f in gc.get_referrers(code)
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_inst_method_calling (__main__.TestFuncs.test_inst_method_calling)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 312, in test_inst_method_calling
    self.tracer.runfunc(obj.inst_method_calling, 1)
  File "/pkg/store/python-0/lib/trace.py", line 467, in runfunc
    result = func(*args, **kw)
  File "/tmp/test_trace.py", line 110, in inst_method_calling
    def inst_method_calling(self, x):
  File "/pkg/store/python-0/lib/trace.py", line 531, in globaltrace_countfuncs
    this_func = self.file_module_function_of(frame)
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
    funcs = [f for f in gc.get_referrers(code)
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_loop_caller_importing (__main__.TestFuncs.test_loop_caller_importing)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 297, in test_loop_caller_importing
    self.tracer.runfunc(traced_func_importing_caller, 1)
  File "/pkg/store/python-0/lib/trace.py", line 467, in runfunc
    result = func(*args, **kw)
  File "/tmp/test_trace.py", line 63, in traced_func_importing_caller
    def traced_func_importing_caller(x):
  File "/pkg/store/python-0/lib/trace.py", line 531, in globaltrace_countfuncs
    this_func = self.file_module_function_of(frame)
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
    funcs = [f for f in gc.get_referrers(code)
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_simple_caller (__main__.TestFuncs.test_simple_caller)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 280, in test_simple_caller
    self.tracer.runfunc(traced_func_simple_caller, 1)
  File "/pkg/store/python-0/lib/trace.py", line 467, in runfunc
    result = func(*args, **kw)
  File "/tmp/test_trace.py", line 59, in traced_func_simple_caller
    def traced_func_simple_caller(x):
  File "/pkg/store/python-0/lib/trace.py", line 531, in globaltrace_countfuncs
    this_func = self.file_module_function_of(frame)
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
    funcs = [f for f in gc.get_referrers(code)
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_traced_decorated_function (__main__.TestFuncs.test_traced_decorated_function)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 322, in test_traced_decorated_function
    self.tracer.runfunc(traced_decorated_function)
  File "/pkg/store/python-0/lib/trace.py", line 467, in runfunc
    result = func(*args, **kw)
  File "/tmp/test_trace.py", line 89, in traced_decorated_function
    def traced_decorated_function():
  File "/pkg/store/python-0/lib/trace.py", line 531, in globaltrace_countfuncs
    this_func = self.file_module_function_of(frame)
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
    funcs = [f for f in gc.get_referrers(code)
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_trace_list_comprehension (__main__.TestLineCounts.test_trace_list_comprehension)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 199, in test_trace_list_comprehension
    self.assertEqual(self.tracer.results().counts, expected)
  File "/pkg/store/python-0/lib/unittest/case.py", line 949, in assertEqual
    assertion_func(first, second, msg=msg)
  File "/pkg/store/python-0/lib/unittest/case.py", line 1245, in assertDictEqual
    pprint.pformat(d1).splitlines(),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_traced_decorated_function (__main__.TestLineCounts.test_traced_decorated_function)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 218, in test_traced_decorated_function
    self.assertEqual(self.tracer.results().counts, expected)
  File "/pkg/store/python-0/lib/unittest/case.py", line 949, in assertEqual
    assertion_func(first, second, msg=msg)
  File "/pkg/store/python-0/lib/unittest/case.py", line 1245, in assertDictEqual
    pprint.pformat(d1).splitlines(),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
FAIL: test_count_and_summary (__main__.TestCommandLine.test_count_and_summary)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 562, in test_count_and_summary
    self.assertIn(f'6   100.0%   {modulename}   ({filename})', stdout)
AssertionError: '6   100.0%   @test_N_tmpæ   (@test_N_tmpæ.py)' not found in 'lines   cov%   module   (path)\n    5   100.0%   @test_N_tmpæ   (@test_N_tmpæ.py)\n'

======================================================================
FAIL: test_listfuncs_flag_success (__main__.TestCommandLine.test_listfuncs_flag_success)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 523, in test_listfuncs_flag_success
    status, stdout, stderr = assert_python_ok('-m', 'trace', '-l', filename,
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-m', 'trace', '-l', '@test_N_tmpæ.py']

stdout:
---

---

stderr:
---
Cannot run file '@test_N_tmpæ.py' because: [Errno 13] Permission denied: '@test_N_tmpæ.py'
---

======================================================================
FAIL: test_run_as_module (__main__.TestCommandLine.test_run_as_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 587, in test_run_as_module
    assert_python_ok('-m', 'trace', '-l', '--module', 'timeit', '-n', '1')
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-I', '-m', 'trace', '-l', '--module', 'timeit', '-n', '1']

stdout:
---

---

stderr:
---
Traceback (most recent call last):
  File "<string>", line 2, in <module>
  File "/pkg/store/python-0/lib/runpy.py", line 201, in _run_module_as_main
  File "/pkg/store/python-0/lib/runpy.py", line 87, in _run_code
  File "/pkg/store/python-0/lib/trace.py", line 754, in <module>
  File "/pkg/store/python-0/lib/trace.py", line 740, in main
  File "/pkg/store/python-0/lib/trace.py", line 456, in runctx
  File "/pkg/store/python-0/lib/timeit.py", line 1, in <module>
  File "/pkg/store/python-0/lib/trace.py", line 531, in globaltrace_countfuncs
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
AttributeError: module 'gc' has no attribute 'get_referrers'
---

======================================================================
FAIL: test_sys_argv_list (__main__.TestCommandLine.test_sys_argv_list)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 536, in test_sys_argv_list
    status, trace_stdout, stderr = assert_python_ok('-m', 'trace', '-l', TESTFN,
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-m', 'trace', '-l', '@test_N_tmpæ']

stdout:
---

---

stderr:
---
Traceback (most recent call last):
  File "<string>", line 2, in <module>
  File "/pkg/store/python-0/lib/runpy.py", line 201, in _run_module_as_main
  File "/pkg/store/python-0/lib/runpy.py", line 87, in _run_code
  File "/pkg/store/python-0/lib/trace.py", line 754, in <module>
  File "/pkg/store/python-0/lib/trace.py", line 740, in main
  File "/pkg/store/python-0/lib/trace.py", line 456, in runctx
  File "@test_N_tmpæ", line 1, in <module>
  File "/pkg/store/python-0/lib/trace.py", line 531, in globaltrace_countfuncs
  File "/pkg/store/python-0/lib/trace.py", line 490, in file_module_function_of
AttributeError: module 'gc' has no attribute 'get_referrers'
---

======================================================================
FAIL: test_cover_files_written_with_highlight (__main__.TestCoverageCommandLineOutput.test_cover_files_written_with_highlight)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_trace.py", line 492, in test_cover_files_written_with_highlight
    status, stdout, stderr = assert_python_ok(*argv)
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-I', '-m', 'trace', '--count', '--missing', 'tmp.py']

stdout:
---

---

stderr:
---
Traceback (most recent call last):
  File "<string>", line 2, in <module>
  File "/pkg/store/python-0/lib/runpy.py", line 201, in _run_module_as_main
  File "/pkg/store/python-0/lib/runpy.py", line 87, in _run_code
  File "/pkg/store/python-0/lib/trace.py", line 754, in <module>
  File "/pkg/store/python-0/lib/trace.py", line 749, in main
  File "/pkg/store/python-0/lib/trace.py", line 272, in write_results
  File "/pkg/store/python-0/lib/trace.py", line 391, in _find_executable_linenos
  File "/pkg/store/python-0/lib/trace.py", line 347, in _find_lines
  File "/pkg/store/python-0/lib/trace.py", line 338, in _find_lines_from_code
AttributeError: module 'dis' has no attribute 'findlinestarts'
---

----------------------------------------------------------------------
Ran 28 tests in Ns

FAILED (failures=5, errors=10, skipped=1)
