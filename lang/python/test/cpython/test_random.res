......E......E......EEs......Es....................EE.F......E...E.....E...E...................................s..ss
======================================================================
ERROR: test_bug_1727780 (__main__.MersenneTwister_TestBasicOps.test_bug_1727780)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 559, in test_bug_1727780
    with open(support.findfile(file),"rb") as f:
FileNotFoundError: [Errno 2] No such file or directory: 'randv2_32.pck'

======================================================================
ERROR: test_choices (__main__.MersenneTwister_TestBasicOps.test_choices)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 274, in test_choices
    choices(data, k=MyIndex(5)),
  File "/pkg/store/python-0/lib/random.py", line 473, in choices
    return [population[floor(random() * n)] for i in _repeat(None, k)]
TypeError: repeat() times must be an integer

======================================================================
ERROR: test_getrandbits (__main__.MersenneTwister_TestBasicOps.test_getrandbits)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 819, in test_getrandbits
    super().test_getrandbits()
  File "/tmp/test_random.py", line 416, in test_getrandbits
    self.assertRaises(OverflowError, getrandbits, 1<<1000)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
TypeError: getrandbits() wants an integer

======================================================================
ERROR: test_getrandbits_2G_bits (__main__.MersenneTwister_TestBasicOps.test_getrandbits_2G_bits)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 832, in test_getrandbits_2G_bits
    x = self.gen.getrandbits(size)
OverflowError: too many bits

======================================================================
ERROR: test_randbytes (__main__.MersenneTwister_TestBasicOps.test_randbytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 923, in test_randbytes
    super().test_randbytes()
  File "/tmp/test_random.py", line 592, in test_randbytes
    self.assertRaises(OverflowError, self.gen.randbytes, 1<<1000)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/random.py", line 289, in randbytes
    return self.getrandbits(n * 8).to_bytes(n, 'little')
TypeError: getrandbits() wants an integer

======================================================================
ERROR: test_seed_when_randomness_source_not_found (__main__.MersenneTwister_TestBasicOps.test_seed_when_randomness_source_not_found)
----------------------------------------------------------------------
TypeError: seed() wants None, an int, a str or bytes: float

======================================================================
ERROR: test_seedargs (__main__.MersenneTwister_TestBasicOps.test_seedargs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 60, in test_seedargs
    self.gen.seed(arg)
  File "/pkg/store/python-0/lib/random.py", line 173, in seed
    super().seed(a)
TypeError: seed() wants None, an int, a str or bytes: float

======================================================================
ERROR: test_bug_1727780 (__main__.SystemRandom_TestBasicOps.test_bug_1727780)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 559, in test_bug_1727780
    with open(support.findfile(file),"rb") as f:
FileNotFoundError: [Errno 2] No such file or directory: 'randv2_32.pck'

======================================================================
ERROR: test_choices (__main__.SystemRandom_TestBasicOps.test_choices)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 274, in test_choices
    choices(data, k=MyIndex(5)),
  File "/pkg/store/python-0/lib/random.py", line 473, in choices
    return [population[floor(random() * n)] for i in _repeat(None, k)]
TypeError: repeat() times must be an integer

======================================================================
ERROR: test_getrandbits (__main__.SystemRandom_TestBasicOps.test_getrandbits)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 416, in test_getrandbits
    self.assertRaises(OverflowError, getrandbits, 1<<1000)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/random.py", line 915, in getrandbits
    x = int.from_bytes(_urandom(numbytes))
TypeError: an integer is required: int

======================================================================
ERROR: test_randbytes (__main__.SystemRandom_TestBasicOps.test_randbytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 592, in test_randbytes
    self.assertRaises(OverflowError, self.gen.randbytes, 1<<1000)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/pkg/store/python-0/lib/random.py", line 922, in randbytes
    return _urandom(n)
TypeError: an integer is required: int

======================================================================
FAIL: test_setstate_middle_arg (__main__.MersenneTwister_TestBasicOps.test_setstate_middle_arg)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_random.py", line 737, in test_setstate_middle_arg
    with self.assertRaises((ValueError, OverflowError)):
AssertionError: (<class 'ValueError'>, <class 'OverflowError'>) not raised

----------------------------------------------------------------------
Ran 116 tests in Ns

FAILED (failures=1, errors=11, skipped=5)
