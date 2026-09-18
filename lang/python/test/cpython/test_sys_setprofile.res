EEEEEEEEEEEEEEEEEEEEEEEEEEEEE
======================================================================
ERROR: test_caught_exception (__main__.ProfileHookTestCase.test_caught_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 129, in test_caught_exception
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_caught_nested_exception (__main__.ProfileHookTestCase.test_caught_nested_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 138, in test_caught_nested_exception
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_distant_exception (__main__.ProfileHookTestCase.test_distant_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 229, in test_distant_exception
    self.check_events(j, [(1, 'call', j_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_exception (__main__.ProfileHookTestCase.test_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 120, in test_exception
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_exception_in_except_clause (__main__.ProfileHookTestCase.test_exception_in_except_clause)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 164, in test_exception_in_except_clause
    self.check_events(g, [(1, 'call', g_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_exception_propagation (__main__.ProfileHookTestCase.test_exception_propagation)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 180, in test_exception_propagation
    self.check_events(g, [(1, 'call', g_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_generator (__main__.ProfileHookTestCase.test_generator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 250, in test_generator
    self.check_events(g, [(1, 'call', g_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_nested_exception (__main__.ProfileHookTestCase.test_nested_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 146, in test_nested_exception
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_raise (__main__.ProfileHookTestCase.test_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 209, in test_raise
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_raise_reraise (__main__.ProfileHookTestCase.test_raise_reraise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 201, in test_raise_reraise
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_raise_twice (__main__.ProfileHookTestCase.test_raise_twice)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 192, in test_raise_twice
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_simple (__main__.ProfileHookTestCase.test_simple)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 112, in test_simple
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_stop_iteration (__main__.ProfileHookTestCase.test_stop_iteration)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 289, in test_stop_iteration
    self.check_events(g, [(1, 'call', g_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_unfinished_generator (__main__.ProfileHookTestCase.test_unfinished_generator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 272, in test_unfinished_generator
    self.check_events(g, [(1, 'call', g_ident, None),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_basic_exception (__main__.ProfileSimulatorTestCase.test_basic_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 318, in test_basic_exception
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_caught_exception (__main__.ProfileSimulatorTestCase.test_caught_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 327, in test_caught_exception
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_distant_exception (__main__.ProfileSimulatorTestCase.test_distant_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 347, in test_distant_exception
    self.check_events(j, [(1, 'call', j_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_simple (__main__.ProfileSimulatorTestCase.test_simple)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 310, in test_simple
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_unbound_method (__main__.ProfileSimulatorTestCase.test_unbound_method)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 365, in test_unbound_method
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_unbound_method_invalid_args (__main__.ProfileSimulatorTestCase.test_unbound_method_invalid_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 381, in test_unbound_method_invalid_args
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_unbound_method_invalid_keyword_args (__main__.ProfileSimulatorTestCase.test_unbound_method_invalid_keyword_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 399, in test_unbound_method_invalid_keyword_args
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_unbound_method_no_args (__main__.ProfileSimulatorTestCase.test_unbound_method_no_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 373, in test_unbound_method_no_args
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_unbound_method_no_keyword_args (__main__.ProfileSimulatorTestCase.test_unbound_method_no_keyword_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 390, in test_unbound_method_no_keyword_args
    self.check_events(f, [(1, 'call', f_ident),
  File "/tmp/test_sys_setprofile.py", line 93, in check_events
    events = capture_events(callable, self.new_watcher())
  File "/tmp/test_sys_setprofile.py", line 426, in capture_events
    sys.setprofile(p.callback)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_method_with_c_function (__main__.TestEdgeCases.test_method_with_c_function)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 443, in setUp
    self.addCleanup(sys.setprofile, sys.getprofile())
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_profile_after_trace_opcodes (__main__.TestEdgeCases.test_profile_after_trace_opcodes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 443, in setUp
    self.addCleanup(sys.setprofile, sys.getprofile())
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_reentrancy (__main__.TestEdgeCases.test_reentrancy)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 443, in setUp
    self.addCleanup(sys.setprofile, sys.getprofile())
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_same_object (__main__.TestEdgeCases.test_same_object)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 443, in setUp
    self.addCleanup(sys.setprofile, sys.getprofile())
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_empty (__main__.TestGetProfile.test_empty)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 9, in setUp
    sys.setprofile(None)
AttributeError: module 'sys' has no attribute 'setprofile'

======================================================================
ERROR: test_setget (__main__.TestGetProfile.test_setget)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_setprofile.py", line 9, in setUp
    sys.setprofile(None)
AttributeError: module 'sys' has no attribute 'setprofile'

----------------------------------------------------------------------
Ran 29 tests in Ns

FAILED (errors=29)
