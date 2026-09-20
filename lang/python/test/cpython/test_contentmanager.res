....................................................................................................EE.E............
======================================================================
ERROR: test_set_text_charset_cp949 (__main__.TestRawDataManager.test_set_text_charset_cp949)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contentmanager.py", line 348, in test_set_text_charset_cp949
    raw_data_manager.set_content(m, content, charset='cp949')
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 38, in set_content
    handler(msg, obj, *args, **kw)
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 179, in set_text_content
    cte, payload = _encode_text(string, charset, cte, msg.policy)
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 135, in _encode_text
    lines = string.encode(charset).splitlines()
LookupError: unknown encoding: ks_c_5601-1987

======================================================================
ERROR: test_set_text_charset_euc_jp (__main__.TestRawDataManager.test_set_text_charset_euc_jp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contentmanager.py", line 388, in test_set_text_charset_euc_jp
    raw_data_manager.set_content(m, content, charset='euc-jp')
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 38, in set_content
    handler(msg, obj, *args, **kw)
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 179, in set_text_content
    cte, payload = _encode_text(string, charset, cte, msg.policy)
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 135, in _encode_text
    lines = string.encode(charset).splitlines()
LookupError: unknown encoding: iso-2022-jp

======================================================================
ERROR: test_set_text_charset_shift_jis (__main__.TestRawDataManager.test_set_text_charset_shift_jis)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_contentmanager.py", line 368, in test_set_text_charset_shift_jis
    raw_data_manager.set_content(m, content, charset='shift_jis')
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 38, in set_content
    handler(msg, obj, *args, **kw)
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 179, in set_text_content
    cte, payload = _encode_text(string, charset, cte, msg.policy)
  File "/pkg/store/python-0/lib/email/contentmanager.py", line 135, in _encode_text
    lines = string.encode(charset).splitlines()
LookupError: unknown encoding: iso-2022-jp

----------------------------------------------------------------------
Ran 116 tests in Ns

FAILED (errors=3)
