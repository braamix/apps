..........EEEEEsEFs....EsEEEEEEEEEs..sEE....ssssssssssss........EEEE....EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE.EEEEEEEEEEEEEEEEEEEEF........E
======================================================================
ERROR: test_1000_bytes (__main__.ChardataBufferTest.test_1000_bytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 763, in test_1000_bytes
    self.assertEqual(self.small_buffer_test(1000), 1)
  File "/tmp/test_pyexpat.py", line 841, in small_buffer_test
    parser.Parse(xml)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_1025_bytes (__main__.ChardataBufferTest.test_1025_bytes)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 760, in test_1025_bytes
    self.assertEqual(self.small_buffer_test(1025), 2)
  File "/tmp/test_pyexpat.py", line 841, in small_buffer_test
    parser.Parse(xml)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_change_size_1 (__main__.ChardataBufferTest.test_change_size_1)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 854, in test_change_size_1
    parser.Parse(xml1, False)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_change_size_2 (__main__.ChardataBufferTest.test_change_size_2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 870, in test_change_size_2
    parser.Parse(xml1, False)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_disabling_buffer (__main__.ChardataBufferTest.test_disabling_buffer)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 812, in test_disabling_buffer
    parser.Parse(xml1, False)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_unchanged_size (__main__.ChardataBufferTest.test_unchanged_size)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 788, in test_unchanged_size
    parser.Parse(xml1)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_set_activation_threshold__fail_for_subparser (__main__.ExpansionProtectionTest.test_set_activation_threshold__fail_for_subparser)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1247, in test_set_activation_threshold__fail_for_subparser
    self.assert_root_parser_failure(setter, 12345)
  File "/tmp/test_pyexpat.py", line 1201, in assert_root_parser_failure
    self.assertRaisesRegex(expat.ExpatError, msg, func, *args, **kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 1438, in assertRaisesRegex
    return context.handle('assertRaisesRegex', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_pyexpat.py", line 1296, in set_activation_threshold
    return parser.SetBillionLaughsAttackProtectionActivationThreshold(threshold)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionActivationThreshold'

======================================================================
ERROR: test_set_activation_threshold__invalid_threshold_type (__main__.ExpansionProtectionTest.test_set_activation_threshold__invalid_threshold_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1232, in test_set_activation_threshold__invalid_threshold_type
    self.assertRaises(TypeError, setter, 1.0)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_pyexpat.py", line 1296, in set_activation_threshold
    return parser.SetBillionLaughsAttackProtectionActivationThreshold(threshold)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionActivationThreshold'

======================================================================
ERROR: test_set_activation_threshold__threshold_not_reached (__main__.ExpansionProtectionTest.test_set_activation_threshold__threshold_not_reached)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1314, in test_set_activation_threshold__threshold_not_reached
    self.set_activation_threshold(parser, pow(10, 5))
  File "/tmp/test_pyexpat.py", line 1296, in set_activation_threshold
    return parser.SetBillionLaughsAttackProtectionActivationThreshold(threshold)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionActivationThreshold'

======================================================================
ERROR: test_set_activation_threshold__threshold_reached (__main__.ExpansionProtectionTest.test_set_activation_threshold__threshold_reached)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1304, in test_set_activation_threshold__threshold_reached
    self.set_activation_threshold(parser, 3)
  File "/tmp/test_pyexpat.py", line 1296, in set_activation_threshold
    return parser.SetBillionLaughsAttackProtectionActivationThreshold(threshold)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionActivationThreshold'

======================================================================
ERROR: test_set_maximum_amplification__amplification_exceeded (__main__.ExpansionProtectionTest.test_set_maximum_amplification__amplification_exceeded)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1324, in test_set_maximum_amplification__amplification_exceeded
    self.set_activation_threshold(parser, 0)
  File "/tmp/test_pyexpat.py", line 1296, in set_activation_threshold
    return parser.SetBillionLaughsAttackProtectionActivationThreshold(threshold)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionActivationThreshold'

======================================================================
ERROR: test_set_maximum_amplification__amplification_not_exceeded (__main__.ExpansionProtectionTest.test_set_maximum_amplification__amplification_not_exceeded)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1334, in test_set_maximum_amplification__amplification_not_exceeded
    self.set_activation_threshold(parser, 0)
  File "/tmp/test_pyexpat.py", line 1296, in set_activation_threshold
    return parser.SetBillionLaughsAttackProtectionActivationThreshold(threshold)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionActivationThreshold'

======================================================================
ERROR: test_set_maximum_amplification__fail_for_subparser (__main__.ExpansionProtectionTest.test_set_maximum_amplification__fail_for_subparser)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1281, in test_set_maximum_amplification__fail_for_subparser
    self.assert_root_parser_failure(setter, 123.45)
  File "/tmp/test_pyexpat.py", line 1201, in assert_root_parser_failure
    self.assertRaisesRegex(expat.ExpatError, msg, func, *args, **kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 1438, in assertRaisesRegex
    return context.handle('assertRaisesRegex', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_pyexpat.py", line 1299, in set_maximum_amplification
    return parser.SetBillionLaughsAttackProtectionMaximumAmplification(max_factor)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionMaximumAmplification'

======================================================================
ERROR: test_set_maximum_amplification__infinity (__main__.ExpansionProtectionTest.test_set_maximum_amplification__infinity)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1260, in test_set_maximum_amplification__infinity
    self.assertIsNone(self.set_maximum_amplification(parser, inf))
  File "/tmp/test_pyexpat.py", line 1299, in set_maximum_amplification
    return parser.SetBillionLaughsAttackProtectionMaximumAmplification(max_factor)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionMaximumAmplification'

======================================================================
ERROR: test_set_maximum_amplification__invalid_max_factor_range (__main__.ExpansionProtectionTest.test_set_maximum_amplification__invalid_max_factor_range)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1274, in test_set_maximum_amplification__invalid_max_factor_range
    self.assertRaisesRegex(expat.ExpatError, msg, setter, float('nan'))
  File "/pkg/store/python-0/lib/unittest/case.py", line 1438, in assertRaisesRegex
    return context.handle('assertRaisesRegex', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_pyexpat.py", line 1299, in set_maximum_amplification
    return parser.SetBillionLaughsAttackProtectionMaximumAmplification(max_factor)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionMaximumAmplification'

======================================================================
ERROR: test_set_maximum_amplification__invalid_max_factor_type (__main__.ExpansionProtectionTest.test_set_maximum_amplification__invalid_max_factor_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 1266, in test_set_maximum_amplification__invalid_max_factor_type
    self.assertRaises(TypeError, setter, None)
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
  File "/tmp/test_pyexpat.py", line 1299, in set_maximum_amplification
    return parser.SetBillionLaughsAttackProtectionMaximumAmplification(max_factor)
AttributeError: 'pyexpat.xmlparser' object has no attribute 'SetBillionLaughsAttackProtectionMaximumAmplification'

======================================================================
ERROR: test_invalid_ExternalEntityRefHandler (__main__.HandlerExceptionTest.test_invalid_ExternalEntityRefHandler)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 700, in test_invalid_ExternalEntityRefHandler
    self.assertEqual(cm.exception.__notes__, notes)
AttributeError: 'TypeError' object has no attribute '__notes__'. Did you mean '.__ne__' instead of '.__notes__'?

======================================================================
ERROR: test_invalid_NotStandalone (__main__.HandlerExceptionTest.test_invalid_NotStandalone)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 687, in test_invalid_NotStandalone
    self.assertEqual(cm.exception.__notes__, notes)
AttributeError: 'TypeError' object has no attribute '__notes__'. Did you mean '.__ne__' instead of '.__notes__'?

======================================================================
ERROR: test_multibyte_encoding_errors (__main__.ParseTest.test_multibyte_encoding_errors) (sample=b'<x> \xa1</x>', exception=<class 'UnicodeDecodeError'>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 438, in test_multibyte_encoding_errors
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_multibyte_encoding_errors (__main__.ParseTest.test_multibyte_encoding_errors) (sample=b'<x> \xa1</x', exception=<class 'UnicodeDecodeError'>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 438, in test_multibyte_encoding_errors
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_non_text_encodings (__main__.ParseTest.test_non_text_encodings) (encoding='hex_codec')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 415, in test_non_text_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_non_text_encodings (__main__.ParseTest.test_non_text_encodings) (encoding='rot_13')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 415, in test_non_text_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-1')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-2')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-3')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-4')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-5')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-6')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-7')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-9')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-10')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-13')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-14')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-15')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='iso8859-16')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp437')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp720')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp737')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp775')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp850')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp852')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp855')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp856')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp857')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp858')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp860')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp861')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp862')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp863')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp865')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp866')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp869')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp874')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1006')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1125')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1250')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1251')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1252')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1253')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1254')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1255')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1256')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1257')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp1258')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='mac-cyrillic')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='mac-greek')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='mac-iceland')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='mac-latin2')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='mac-roman')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='mac-turkish')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='koi8-r')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='koi8-t')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='koi8-u')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='kz1048')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='ptcp154')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp932')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: cp932

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp949')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: cp949

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='cp950')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: cp950

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='Big5')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: Big5

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='EUC-JP')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: EUC-JP

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='GB2312')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: GB2312

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='GBK')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: GBK

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='johab')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: johab

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='Shift_JIS')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: Shift_JIS

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='UTF8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='utf-8-sig')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 307, in test_supported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 1

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='Big5-HKSCS')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: Big5-HKSCS

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='EUC_JIS-2004')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: EUC_JIS-2004

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='EUC_JISX0213')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: EUC_JISX0213

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='Shift_JIS-2004')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: Shift_JIS-2004

======================================================================
ERROR: test_supported_encodings (__main__.ParseTest.test_supported_encodings) (encoding='Shift_JISX0213')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 304, in test_supported_encodings
    c = 'éπя\u05d0\u060c€'.encode(encoding, 'ignore').decode(encoding)[0]
LookupError: unknown encoding: Shift_JISX0213

======================================================================
ERROR: test_supported_encodings2 (__main__.ParseTest.test_supported_encodings2) (encoding='utf8')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 326, in test_supported_encodings2
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings2 (__main__.ParseTest.test_supported_encodings2) (encoding='koi8-u')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 326, in test_supported_encodings2
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings2 (__main__.ParseTest.test_supported_encodings2) (encoding='cp1125')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 326, in test_supported_encodings2
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings2 (__main__.ParseTest.test_supported_encodings2) (encoding='cp1251')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 326, in test_supported_encodings2
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings2 (__main__.ParseTest.test_supported_encodings2) (encoding='iso8859-5')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 326, in test_supported_encodings2
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_supported_encodings2 (__main__.ParseTest.test_supported_encodings2) (encoding='mac-cyrillic')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 326, in test_supported_encodings2
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 0

======================================================================
ERROR: test_undefined_encoding (__main__.ParseTest.test_undefined_encoding)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 421, in test_undefined_encoding
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_unknown_encoding (__main__.ParseTest.test_unknown_encoding)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 427, in test_unknown_encoding
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='UTF-7')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 381, in test_unsupported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='unicode-escape')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 381, in test_unsupported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='raw-unicode-escape')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 381, in test_unsupported_encodings
    parser.Parse(data, True)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='EUC-KR')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: EUC-KR

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='GB18030')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: GB18030

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='HZ-GB-2312')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: HZ-GB-2312

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='ISO-2022-JP')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: ISO-2022-JP

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='ISO-2022-JP-1')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: ISO-2022-JP-1

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='ISO-2022-JP-2004')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: ISO-2022-JP-2004

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='ISO-2022-JP-2')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: ISO-2022-JP-2

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='ISO-2022-JP-3')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: ISO-2022-JP-3

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='ISO-2022-JP-EXT')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: ISO-2022-JP-EXT

======================================================================
ERROR: test_unsupported_encodings (__main__.ParseTest.test_unsupported_encodings) (encoding='ISO-2022-KR')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 379, in test_unsupported_encodings
    '<root></root>').encode(encoding)
LookupError: unknown encoding: ISO-2022-KR

======================================================================
ERROR: test_unsupported_non_bmp (__main__.ParseTest.test_unsupported_non_bmp) (encoding='Big5-HKSCS')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 362, in test_unsupported_non_bmp
    f'<root>{c}</root>').encode(encoding)
LookupError: unknown encoding: Big5-HKSCS

======================================================================
ERROR: test_unsupported_non_bmp (__main__.ParseTest.test_unsupported_non_bmp) (encoding='EUC_JIS-2004')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 362, in test_unsupported_non_bmp
    f'<root>{c}</root>').encode(encoding)
LookupError: unknown encoding: EUC_JIS-2004

======================================================================
ERROR: test_unsupported_non_bmp (__main__.ParseTest.test_unsupported_non_bmp) (encoding='EUC_JISX0213')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 362, in test_unsupported_non_bmp
    f'<root>{c}</root>').encode(encoding)
LookupError: unknown encoding: EUC_JISX0213

======================================================================
ERROR: test_unsupported_non_bmp (__main__.ParseTest.test_unsupported_non_bmp) (encoding='Shift_JIS-2004')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 362, in test_unsupported_non_bmp
    f'<root>{c}</root>').encode(encoding)
LookupError: unknown encoding: Shift_JIS-2004

======================================================================
ERROR: test_unsupported_non_bmp (__main__.ParseTest.test_unsupported_non_bmp) (encoding='Shift_JISX0213')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_pyexpat.py", line 362, in test_unsupported_non_bmp
    f'<root>{c}</root>').encode(encoding)
LookupError: unknown encoding: Shift_JISX0213

======================================================================
ERROR: test_parse_only_xml_data (__main__.sf1296433Test.test_parse_only_xml_data)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 752, in test_parse_only_xml_data
    self.assertRaises(SpecificException, parser.Parse, xml.encode('iso8859'))
  File "/pkg/store/python-0/lib/unittest/case.py", line 835, in assertRaises
    return context.handle('assertRaises', args, kwargs)
  File "/pkg/store/python-0/lib/unittest/case.py", line 245, in handle
    callable_obj(*args, **kwargs)
pyexpat.ExpatError: unknown encoding: line 1, column 30

======================================================================
FAIL: test_wrong_size (__main__.ChardataBufferTest.test_wrong_size)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 772, in test_wrong_size
    with self.assertRaises((ValueError, OverflowError)):
AssertionError: (<class 'ValueError'>, <class 'OverflowError'>) not raised

======================================================================
FAIL: test (__main__.PositionTest.test)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pyexpat.py", line 732, in test
    self.parser.Parse(xml, True)
  File "/tmp/test_pyexpat.py", line 709, in EndElementHandler
    self.check_pos('e')
  File "/tmp/test_pyexpat.py", line 719, in check_pos
    self.assertEqual(pos, expected,
AssertionError: Tuples differ: ('e', 11, 3, 2) != ('e', 15, 3, 6)

First differing element 1:
11
15

- ('e', 11, 3, 2)
?        ^     ^

+ ('e', 15, 3, 6)
?        ^     ^
 : Expected position ('e', 11, 3, 2), got position ('e', 15, 3, 6)

----------------------------------------------------------------------
Ran 86 tests in Ns

FAILED (failures=2, errors=119, skipped=17)
