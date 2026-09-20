...............................................................................FE......E....E.......................E.....................................E.......E................................................................s.....................................................................................................................sEEE.....................................................................................................................................................................F.............................
======================================================================
ERROR: test_body_encode (__main__.TestCharset.test_body_encode)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 5143, in test_body_encode
    c.body_encode('\u83ca\u5730\u6642\u592b'))
  File "/pkg/store/python-0/lib/email/charset.py", line 441, in body_encode
    string = string.encode(self.output_charset).decode('ascii')
LookupError: unknown encoding: iso-2022-jp

======================================================================
ERROR: test_encode7or8bit (__main__.TestEncoders.test_encode7or8bit)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 924, in test_encode7or8bit
    msg = MIMEText('文\n', _charset='euc-jp')
  File "/pkg/store/python-0/lib/email/mime/text.py", line 40, in __init__
    self.set_payload(_text, _charset)
  File "/pkg/store/python-0/lib/email/message.py", line 355, in set_payload
    payload = payload.encode(charset.output_charset, 'surrogateescape')
LookupError: unknown encoding: iso-2022-jp

======================================================================
ERROR: test_long_lines (__main__.TestFeedParsers.test_long_lines)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 3918, in test_long_lines
    m = self.parse(['a:b\n\n'] + ['x'*M] * N)
  File "/tmp/test_email.py", line 3888, in parse
    feedparser.feed(chunk)
  File "/pkg/store/python-0/lib/email/feedparser.py", line 175, in feed
    self._input.push(data)
  File "/pkg/store/python-0/lib/email/feedparser.py", line 104, in push
    self._partial.write(data)
MemoryError: out of memory

======================================================================
ERROR: test_shift_jis_charset (__main__.TestHeader.test_shift_jis_charset)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 5502, in test_shift_jis_charset
    h = Header('文', charset='shift_jis')
  File "/pkg/store/python-0/lib/email/header.py", line 222, in __init__
    self.append(s, charset, errors)
  File "/pkg/store/python-0/lib/email/header.py", line 306, in append
    s.encode(output_charset, errors)
LookupError: unknown encoding: iso-2022-jp

======================================================================
ERROR: test_header_encode_with_different_output_charset (__main__.TestLongHeaders.test_header_encode_with_different_output_charset)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 1082, in test_header_encode_with_different_output_charset
    h = Header('文', 'euc-jp')
  File "/pkg/store/python-0/lib/email/header.py", line 222, in __init__
    self.append(s, charset, errors)
  File "/pkg/store/python-0/lib/email/header.py", line 306, in append
    s.encode(output_charset, errors)
LookupError: unknown encoding: iso-2022-jp

======================================================================
ERROR: test_long_header_encode_with_different_output_charset (__main__.TestLongHeaders.test_long_header_encode_with_different_output_charset)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 1089, in test_long_header_encode_with_different_output_charset
    b'\xa4\xa4\xde\xa4\xb9'.decode('euc-jp'), 'euc-jp')
LookupError: unknown encoding: euc-jp

======================================================================
ERROR: test_make_msgid_default_domain (__main__.TestMiscellaneous.test_make_msgid_default_domain)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 3671, in test_make_msgid_default_domain
    with patch('socket.getfqdn') as mock_getfqdn:
  File "/pkg/store/python-0/lib/unittest/mock.py", line 1492, in __enter__
    self.target = self.getter()
  File "/pkg/store/python-0/lib/pkgutil.py", line 476, in resolve_name
    mod = importlib.import_module(modname)
  File "/pkg/store/python-0/lib/importlib/__init__.py", line 88, in import_module
    return _bootstrap._gcd_import(name[level:], package, level)
  File "/pkg/store/python-0/lib/importlib/_bootstrap.py", line 1384, in _gcd_import
    return _find_and_load(name, _gcd_import)
  File "/pkg/store/python-0/lib/importlib/_bootstrap.py", line 1344, in _find_and_load
    return _find_and_load_unlocked(
  File "/pkg/store/python-0/lib/importlib/_bootstrap.py", line 1296, in _find_and_load_unlocked
    raise ModuleNotFoundError(f'{_ERR_MSG_PREFIX}{name!r}', name=name)
ModuleNotFoundError: Standard library module 'socket' was not found

======================================================================
ERROR: test_make_msgid_domain (__main__.TestMiscellaneous.test_make_msgid_domain)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 3661, in test_make_msgid_domain
    email.utils.make_msgid(domain='testdomain-string')[-19:],
  File "/pkg/store/python-0/lib/email/utils.py", line 307, in make_msgid
    import socket
ModuleNotFoundError: Standard library module 'socket' was not found

======================================================================
ERROR: test_make_msgid_idstring (__main__.TestMiscellaneous.test_make_msgid_idstring)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 3666, in test_make_msgid_idstring
    email.utils.make_msgid(idstring='test-idstring',
  File "/pkg/store/python-0/lib/email/utils.py", line 307, in make_msgid
    import socket
ModuleNotFoundError: Standard library module 'socket' was not found

======================================================================
FAIL: test_attributes (__main__.TestCharset.test_attributes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 5062, in test_attributes
    self.assertEqual(c.input_charset, 'euc-jp')
AssertionError: 'eucjp' != 'euc-jp'
- eucjp
+ euc-jp
?    +


======================================================================
FAIL: test_rfc2231_bad_character_in_encoding (__main__.TestRFC2231.test_rfc2231_bad_character_in_encoding)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_email.py", line 5805, in test_rfc2231_bad_character_in_encoding
    with warnings_helper.check_warnings(('', DeprecationWarning)):
  File "/pkg/store/python-0/lib/contextlib.py", line 210, in __exit__
    for _ in self.gen:
  File "/tmp/test/support/warnings_helper.py", line 107, in _filterwarnings
    raise AssertionError("filter (%r, %s) did not catch any warning" % missing[0])
AssertionError: filter ('', DeprecationWarning) did not catch any warning

----------------------------------------------------------------------
Ran 544 tests in Ns

FAILED (failures=2, errors=9, skipped=2)
