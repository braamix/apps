

...............E...................................................................
======================================================================
ERROR: test_idna_encoding_preserved (__main__.TestBytesGenerator.test_idna_encoding_preserved)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_generator.py", line 645, in test_idna_encoding_preserved
    domain='☕.example'.encode('idna').decode()  # IDNA 2003
LookupError: unknown encoding: idna

----------------------------------------------------------------------
Ran 83 tests in Ns

FAILED (errors=1)
