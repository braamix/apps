...........................................................................................................................F..................................................................F...
======================================================================
FAIL: test_value_rfc2231_nonascii_in_charset_of_charset_parameter_value (__main__.TestContentTypeHeader.test_value_rfc2231_nonascii_in_charset_of_charset_parameter_value)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/test_email/__init__.py", line 160, in <lambda>
    getattr(self, name)(*params))
  File "/tmp/test_headerregistry.py", line 264, in content_type_as_value
    with warnings_helper.check_warnings(('', DeprecationWarning)):
  File "/pkg/store/python-0/lib/contextlib.py", line 210, in __exit__
    for _ in self.gen:
  File "/tmp/test/support/warnings_helper.py", line 107, in _filterwarnings
    raise AssertionError("filter (%r, %s) did not catch any warning" % missing[0])
AssertionError: filter ('', DeprecationWarning) did not catch any warning

======================================================================
FAIL: test_value_rfc2047_gb2312_base64 (__main__.TestUnstructuredHeader.test_value_rfc2047_gb2312_base64)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/test_email/__init__.py", line 160, in <lambda>
    getattr(self, name)(*params))
  File "/tmp/test_headerregistry.py", line 141, in string_as_value
    self.assertEqual(h, decoded)
AssertionError: '�������Ĳ��ԣ�' != '这是中文测试！'

----------------------------------------------------------------------
Ran 194 tests in Ns

FAILED (failures=2)
