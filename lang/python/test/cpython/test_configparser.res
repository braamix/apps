.....................................................................................................s..............................................................ss.................................................................E..EEEEEE..........................................................................ss...................................................................
======================================================================
ERROR: test_duplicateoptionerror (__main__.ExceptionPicklingTestCase.test_duplicateoptionerror)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_configparser.py", line 1825, in test_duplicateoptionerror
    e2 = pickle.loads(pickled)
  File "/pkg/store/python-0/lib/pickle.py", line 1911, in _loads
    encoding=encoding, errors=errors).load()
  File "/pkg/store/python-0/lib/pickle.py", line 1346, in load
    dispatch[key[0]](self)
  File "/pkg/store/python-0/lib/pickle.py", line 1728, in load_reduce
    stack[-1] = func(*args)
TypeError: DuplicateOptionError.__init__() missing a required positional argument: 'option'

======================================================================
ERROR: test_interpolationdeptherror (__main__.ExceptionPicklingTestCase.test_interpolationdeptherror)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_configparser.py", line 1878, in test_interpolationdeptherror
    e2 = pickle.loads(pickled)
  File "/pkg/store/python-0/lib/pickle.py", line 1911, in _loads
    encoding=encoding, errors=errors).load()
  File "/pkg/store/python-0/lib/pickle.py", line 1346, in load
    dispatch[key[0]](self)
  File "/pkg/store/python-0/lib/pickle.py", line 1728, in load_reduce
    stack[-1] = func(*args)
TypeError: InterpolationDepthError.__init__() missing a required positional argument: 'section'

======================================================================
ERROR: test_interpolationerror (__main__.ExceptionPicklingTestCase.test_interpolationerror)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_configparser.py", line 1839, in test_interpolationerror
    e2 = pickle.loads(pickled)
  File "/pkg/store/python-0/lib/pickle.py", line 1911, in _loads
    encoding=encoding, errors=errors).load()
  File "/pkg/store/python-0/lib/pickle.py", line 1346, in load
    dispatch[key[0]](self)
  File "/pkg/store/python-0/lib/pickle.py", line 1728, in load_reduce
    stack[-1] = func(*args)
TypeError: InterpolationError.__init__() missing a required positional argument: 'section'

======================================================================
ERROR: test_interpolationmissingoptionerror (__main__.ExceptionPicklingTestCase.test_interpolationmissingoptionerror)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_configparser.py", line 1852, in test_interpolationmissingoptionerror
    e2 = pickle.loads(pickled)
  File "/pkg/store/python-0/lib/pickle.py", line 1911, in _loads
    encoding=encoding, errors=errors).load()
  File "/pkg/store/python-0/lib/pickle.py", line 1346, in load
    dispatch[key[0]](self)
  File "/pkg/store/python-0/lib/pickle.py", line 1728, in load_reduce
    stack[-1] = func(*args)
TypeError: InterpolationMissingOptionError.__init__() missing a required positional argument: 'section'

======================================================================
ERROR: test_interpolationsyntaxerror (__main__.ExceptionPicklingTestCase.test_interpolationsyntaxerror)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_configparser.py", line 1865, in test_interpolationsyntaxerror
    e2 = pickle.loads(pickled)
  File "/pkg/store/python-0/lib/pickle.py", line 1911, in _loads
    encoding=encoding, errors=errors).load()
  File "/pkg/store/python-0/lib/pickle.py", line 1346, in load
    dispatch[key[0]](self)
  File "/pkg/store/python-0/lib/pickle.py", line 1728, in load_reduce
    stack[-1] = func(*args)
TypeError: InterpolationError.__init__() missing a required positional argument: 'section'

======================================================================
ERROR: test_missingsectionheadererror (__main__.ExceptionPicklingTestCase.test_missingsectionheadererror)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_configparser.py", line 1917, in test_missingsectionheadererror
    e2 = pickle.loads(pickled)
  File "/pkg/store/python-0/lib/pickle.py", line 1911, in _loads
    encoding=encoding, errors=errors).load()
  File "/pkg/store/python-0/lib/pickle.py", line 1346, in load
    dispatch[key[0]](self)
  File "/pkg/store/python-0/lib/pickle.py", line 1728, in load_reduce
    stack[-1] = func(*args)
TypeError: MissingSectionHeaderError.__init__() missing a required positional argument: 'lineno'

======================================================================
ERROR: test_nooptionerror (__main__.ExceptionPicklingTestCase.test_nooptionerror)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_configparser.py", line 1799, in test_nooptionerror
    e2 = pickle.loads(pickled)
  File "/pkg/store/python-0/lib/pickle.py", line 1911, in _loads
    encoding=encoding, errors=errors).load()
  File "/pkg/store/python-0/lib/pickle.py", line 1346, in load
    dispatch[key[0]](self)
  File "/pkg/store/python-0/lib/pickle.py", line 1728, in load_reduce
    stack[-1] = func(*args)
TypeError: NoOptionError.__init__() missing a required positional argument: 'section'

----------------------------------------------------------------------
Ran 383 tests in Ns

FAILED (errors=7, skipped=5)
