xxEEEE.Ex..
======================================================================
ERROR: test_expandItem (__main__.PullDOMTestCase.test_expandItem)
Ensure expandItem works as expected.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pulldom.py", line 107, in test_expandItem
    items = pulldom.parseString(SMALL_SAMPLE)
  File "/pkg/store/python-0/lib/xml/dom/pulldom.py", line 349, in parseString
    parser = xml.sax.make_parser()
  File "/pkg/store/python-0/lib/xml/sax/__init__.py", line 97, in make_parser
    raise SAXReaderNotAvailable("No parsers found", None)
xml.sax._exceptions.SAXReaderNotAvailable: No parsers found

======================================================================
ERROR: test_external_ges_default (__main__.PullDOMTestCase.test_external_ges_default)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pulldom.py", line 164, in test_external_ges_default
    parser = pulldom.parseString(SMALL_SAMPLE)
  File "/pkg/store/python-0/lib/xml/dom/pulldom.py", line 349, in parseString
    parser = xml.sax.make_parser()
  File "/pkg/store/python-0/lib/xml/sax/__init__.py", line 97, in make_parser
    raise SAXReaderNotAvailable("No parsers found", None)
xml.sax._exceptions.SAXReaderNotAvailable: No parsers found

======================================================================
ERROR: test_parse (__main__.PullDOMTestCase.test_parse)
Minimal test of DOMEventStream.parse()
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pulldom.py", line 35, in test_parse
    handler = pulldom.parse(tstfile)
  File "/pkg/store/python-0/lib/xml/dom/pulldom.py", line 339, in parse
    parser = xml.sax.make_parser()
  File "/pkg/store/python-0/lib/xml/sax/__init__.py", line 97, in make_parser
    raise SAXReaderNotAvailable("No parsers found", None)
xml.sax._exceptions.SAXReaderNotAvailable: No parsers found

======================================================================
ERROR: test_parse_semantics (__main__.PullDOMTestCase.test_parse_semantics)
Test DOMEventStream parsing semantics.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pulldom.py", line 46, in test_parse_semantics
    items = pulldom.parseString(SMALL_SAMPLE)
  File "/pkg/store/python-0/lib/xml/dom/pulldom.py", line 349, in parseString
    parser = xml.sax.make_parser()
  File "/pkg/store/python-0/lib/xml/sax/__init__.py", line 97, in make_parser
    raise SAXReaderNotAvailable("No parsers found", None)
xml.sax._exceptions.SAXReaderNotAvailable: No parsers found

======================================================================
ERROR: test_basic (__main__.SAX2DOMTestCase.test_basic)
Ensure SAX2DOM can parse from a stream.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pulldom.py", line 304, in test_basic
    sd = SAX2DOMTestHelper(fin, xml.sax.make_parser(),
  File "/pkg/store/python-0/lib/xml/sax/__init__.py", line 97, in make_parser
    raise SAXReaderNotAvailable("No parsers found", None)
xml.sax._exceptions.SAXReaderNotAvailable: No parsers found

----------------------------------------------------------------------
Ran 11 tests in Ns

FAILED (errors=5, expected failures=3)
