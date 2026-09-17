.F..FE..
======================================================================
ERROR: test_6 (__main__.TestPkg.test_6)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pkg.py", line 235, in test_6
    self.run_code(s)
  File "/tmp/test_pkg.py", line 70, in run_code
    exec(textwrap.dedent(code), globals(), {"self": self})
  File "<string>", line 3, in <module>
ImportError: __all__ names nothing: spam

======================================================================
FAIL: test_2 (__main__.TestPkg.test_2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pkg.py", line 123, in test_2
    self.run_code(s)
  File "/tmp/test_pkg.py", line 70, in run_code
    exec(textwrap.dedent(code), globals(), {"self": self})
  File "<string>", line 4, in <module>
AssertionError: Lists differ: ['__doc__', 'self', 'sub', 't2'] != ['self', 'sub', 't2']

First differing element 0:
'__doc__'
'self'

First list contains 1 additional elements.
First extra element 3:
't2'

- ['__doc__', 'self', 'sub', 't2']
?  -----------

+ ['self', 'sub', 't2']

======================================================================
FAIL: test_5 (__main__.TestPkg.test_5)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pkg.py", line 197, in test_5
    self.run_code(s)
  File "/tmp/test_pkg.py", line 70, in run_code
    exec(textwrap.dedent(code), globals(), {"self": self})
  File "<string>", line 3, in <module>
AssertionError: Lists differ: ['__doc__', 'foo', 'self', 'string', 't5'] != ['foo', 'self', 'string', 't5']

First differing element 0:
'__doc__'
'foo'

First list contains 1 additional elements.
First extra element 4:
't5'

- ['__doc__', 'foo', 'self', 'string', 't5']
?  -----------

+ ['foo', 'self', 'string', 't5']

----------------------------------------------------------------------
Ran 8 tests in Ns

FAILED (failures=2, errors=1)
