......E....
======================================================================
ERROR: test_load_global_specialization_failure_keeps_oparg (__main__.RebindBuiltinsTests.test_load_global_specialization_failure_keeps_oparg)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_dynamic.py", line 148, in test_load_global_specialization_failure_keeps_oparg
    sum_func = eval(code, MyGlobals())
TypeError: globals must be a real dict: MyGlobals

----------------------------------------------------------------------
Ran 11 tests in Ns

FAILED (errors=1)
