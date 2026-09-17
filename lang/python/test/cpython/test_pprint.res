EE.EEEE.....E.E.EFEEFEEFEE....E.EEE.EEEE..EE....E.E...E.sE..EEF.F.FEE...EEE.EEEE.E..E
======================================================================
ERROR: test_abc_views (__main__.QueryTestCase.test_abc_views) (length='short', name='Views')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 644, in test_abc_views
    self.assertEqual(pprint.pformat(KeysView(d), sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 894, in _safe_repr
    mapping_repr, readable, recursive = self.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_abc_views (__main__.QueryTestCase.test_abc_views) (length='long', name='Views')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 644, in test_abc_views
    self.assertEqual(pprint.pformat(KeysView(d), sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 894, in _safe_repr
    mapping_repr, readable, recursive = self.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_basic_line_wrap (__main__.QueryTestCase.test_basic_line_wrap)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 343, in test_basic_line_wrap
    self.assertEqual(pprint.pformat(type(o)), exp)
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_bytearray_wrap (__main__.QueryTestCase.test_bytearray_wrap)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1260, in test_bytearray_wrap
    self.assertEqual(pprint.pformat({'a': 1, 'b': letters, 'c': 2},
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_bytes_wrap (__main__.QueryTestCase.test_bytes_wrap)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1204, in test_bytes_wrap
    self.assertEqual(pprint.pformat({'a': 1, 'b': letters, 'c': 2},
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_chainmap (__main__.QueryTestCase.test_chainmap)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1315, in test_chainmap
    self.assertEqual(pprint.pformat(d),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 690, in _pprint_chain_map
    self._format(m, stream, indent, allowance + 1, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_dataclass_no_repr (__main__.QueryTestCase.test_dataclass_no_repr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 828, in test_dataclass_no_repr
    formatted = pprint.pformat(dc, width=10)
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 777, in _pprint_interpolation
    object.value,
AttributeError: 'dataclass3' object has no attribute 'value'

======================================================================
ERROR: test_default_dict (__main__.QueryTestCase.test_default_dict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1279, in test_default_dict
    self.assertEqual(pprint.pformat(d),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 651, in _pprint_default_dict
    self._pprint_dict(object, stream, indent, allowance + 1, context,
  File "/pkg/store/python-0/lib/pprint.py", line 265, in _pprint_dict
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_deque (__main__.QueryTestCase.test_deque)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1410, in test_deque
    self.assertEqual(pprint.pformat(d, width=1), "deque([])")
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 777, in _pprint_interpolation
    object.value,
AttributeError: 'collections.deque' object has no attribute 'value'

======================================================================
ERROR: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='short', prefix='dict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
IndexError: index out of range

======================================================================
ERROR: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='long', prefix='dict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
IndexError: index out of range

======================================================================
ERROR: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='short', prefix='odict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
IndexError: index out of range

======================================================================
ERROR: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='long', prefix='odict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
IndexError: index out of range

======================================================================
ERROR: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='short', prefix='dict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
IndexError: index out of range

======================================================================
ERROR: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='long', prefix='dict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
IndexError: index out of range

======================================================================
ERROR: test_expand_chainmap (__main__.QueryTestCase.test_expand_chainmap)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1887, in test_expand_chainmap
    self.assertEqual(pprint.pformat(dummy_chainmap, width=40, indent=4,
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 695, in _pprint_chain_map
    self._format(m, stream, indent, 1, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 837, in _safe_repr
    vrepr, vreadable, vrecur = self.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_expand_dataclass (__main__.QueryTestCase.test_expand_dataclass)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1617, in test_expand_dataclass
    self.assertEqual(pprint.pformat(dummy_dataclass, width=40, indent=4,
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 215, in _format
    self._pprint_dataclass(object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 253, in _pprint_dataclass
    self._format_namespace_items(items, stream, indent, allowance, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 569, in _format_namespace_items
    self._format(
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_expand_defaultdict (__main__.QueryTestCase.test_expand_defaultdict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1841, in test_expand_defaultdict
    self.assertEqual(pprint.pformat(dummy_defaultdict, width=40, indent=4,
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 651, in _pprint_default_dict
    self._pprint_dict(object, stream, indent, allowance + 1, context,
  File "/pkg/store/python-0/lib/pprint.py", line 265, in _pprint_dict
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_expand_deque (__main__.QueryTestCase.test_expand_deque)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1930, in test_expand_deque
    self.assertEqual(pprint.pformat(dummy_deque, width=40, indent=4,
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 762, in _pprint_interpolation
    ("value", object.value),
AttributeError: 'collections.deque' object has no attribute 'value'

======================================================================
ERROR: test_expand_dict_items (__main__.QueryTestCase.test_expand_dict_items)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 2024, in test_expand_dict_items
    pprint.pformat(d.items(), width=20, indent=4, expand=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_expand_dict_keys (__main__.QueryTestCase.test_expand_dict_keys)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1996, in test_expand_dict_keys
    pprint.pformat(d.keys(), width=20, indent=4, expand=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_expand_dict_values (__main__.QueryTestCase.test_expand_dict_values)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 2010, in test_expand_dict_values
    pprint.pformat(d.values(), width=20, indent=4, expand=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
TypeError: 'int' object is not subscriptable

======================================================================
ERROR: test_expand_frozendict (__main__.QueryTestCase.test_expand_frozendict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1760, in test_expand_frozendict
    pprint.pformat(dummy_frozendict, width=20, indent=4, expand=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_expand_mappingproxy (__main__.QueryTestCase.test_expand_mappingproxy)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1800, in test_expand_mappingproxy
    self.assertEqual(pprint.pformat(dummy_mappingproxy, width=40, indent=4,
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 762, in _pprint_interpolation
    ("value", object.value),
AttributeError: 'mappingproxy' object has no attribute 'value'

======================================================================
ERROR: test_expand_namespace (__main__.QueryTestCase.test_expand_namespace)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1822, in test_expand_namespace
    self.assertEqual(pprint.pformat(dummy_namespace, width=40, indent=4,
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 762, in _pprint_interpolation
    ("value", object.value),
AttributeError: 'types.SimpleNamespace' object has no attribute 'value'

======================================================================
ERROR: test_expand_template (__main__.QueryTestCase.test_expand_template)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1559, in test_expand_template
    pprint.pformat(d, width=40, indent=4, expand=True),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 762, in _pprint_interpolation
    ("value", object.value),
AttributeError: 'string.templatelib.Template' object has no attribute 'value'

======================================================================
ERROR: test_expand_userdict (__main__.QueryTestCase.test_expand_userdict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1962, in test_expand_userdict
    self.assertEqual(pprint.pformat(dummy_userdict, width=40, indent=4,
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 724, in _pprint_user_dict
    self._format(object.data, stream, indent, allowance, context, level - 1)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_knotted (__main__.QueryTestCase.test_knotted)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 211, in test_knotted
    self.assertTrue(pprint.isrecursive(icky), "expected isrecursive")
  File "/pkg/store/python-0/lib/pprint.py", line 83, in isrecursive
    return PrettyPrinter()._safe_repr(object, {}, None, 0)[2]
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_mapping_proxy (__main__.QueryTestCase.test_mapping_proxy)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 572, in test_mapping_proxy
    self.assertEqual(pprint.pformat(m), """\
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 777, in _pprint_interpolation
    object.value,
AttributeError: 'mappingproxy' object has no attribute 'value'

======================================================================
ERROR: test_nested_indentations (__main__.QueryTestCase.test_nested_indentations)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 443, in test_nested_indentations
    self.assertEqual(pprint.pformat(o, indent=4, width=42), expected)
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 873, in _safe_repr
    orepr, oreadable, orecur = self.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_nested_views (__main__.QueryTestCase.test_nested_views)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 677, in test_nested_views
    self.assertEqual(pprint.pformat(d3),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 837, in _safe_repr
    vrepr, vreadable, vrecur = self.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 902, in _safe_repr
    object = sorted(object, key=key)
  File "/pkg/store/python-0/lib/pprint.py", line 111, in _safe_tuple
    return _safe_key(t[0]), _safe_key(t[1])
TypeError: 'dict_view' object is not subscriptable

======================================================================
ERROR: test_simple_namespace (__main__.QueryTestCase.test_simple_namespace)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 763, in test_simple_namespace
    formatted = pprint.pformat(ns, width=60, indent=4)
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 777, in _pprint_interpolation
    object.value,
AttributeError: 'types.SimpleNamespace' object has no attribute 'value'

======================================================================
ERROR: test_simple_namespace_subclass (__main__.QueryTestCase.test_simple_namespace_subclass)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 788, in test_simple_namespace_subclass
    formatted = pprint.pformat(ns, width=60)
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 777, in _pprint_interpolation
    object.value,
AttributeError: 'types.SimpleNamespace' object has no attribute 'value'

======================================================================
ERROR: test_sort_orderable_and_unorderable_values (__main__.QueryTestCase.test_sort_orderable_and_unorderable_values)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1080, in test_sort_orderable_and_unorderable_values
    self.assertEqual(pprint.pformat(set([b, a]), width=1),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 397, in _pprint_set
    self._format_items(object, stream, indent, allowance + len(endchar),
  File "/pkg/store/python-0/lib/pprint.py", line 618, in _format_items
    self._format(ent, stream, indent,
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 777, in _pprint_interpolation
    object.value,
AttributeError: 'Orderable' object has no attribute 'value'

======================================================================
ERROR: test_sort_unorderable_values (__main__.QueryTestCase.test_sort_unorderable_values)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1054, in test_sort_unorderable_values
    self.assertEqual(clean(pprint.pformat(dict.fromkeys(keys))),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_sorted_dict (__main__.QueryTestCase.test_sorted_dict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 507, in test_sorted_dict
    self.assertEqual(pprint.pformat(d), "{'a': 1, 'b': 1, 'c': 1}")
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_str_wrap (__main__.QueryTestCase.test_str_wrap)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1097, in test_str_wrap
    self.assertEqual(pprint.pformat({'a': 1, 'b': fox, 'c': 2},
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_subclassing (__main__.QueryTestCase.test_subclassing)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 862, in test_subclassing
    self.assertEqual(dotted_printer.pformat(o), exp)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/tmp/test_pprint.py", line 2059, in format
    return pprint.PrettyPrinter.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_template (__main__.QueryTestCase.test_template)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1523, in test_template
    self.assertEqual(pprint.pformat(d, width=1),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 777, in _pprint_interpolation
    object.value,
AttributeError: 'string.templatelib.Template' object has no attribute 'value'

======================================================================
ERROR: test_unorderable_items_views (__main__.QueryTestCase.test_unorderable_items_views)
Check that views with unorderable items have stable sorting.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 686, in test_unorderable_items_views
    self.assertEqual(pprint.pformat(iv),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 894, in _safe_repr
    mapping_repr, readable, recursive = self.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_user_dict (__main__.QueryTestCase.test_user_dict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 1443, in test_user_dict
    self.assertEqual(pprint.pformat(d),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 205, in _format
    p(self, object, stream, indent, allowance, context, level + 1)
  File "/pkg/store/python-0/lib/pprint.py", line 724, in _pprint_user_dict
    self._format(object.data, stream, indent, allowance, context, level - 1)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
ERROR: test_width (__main__.QueryTestCase.test_width)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 463, in test_width
    self.assertEqual(pprint.pformat(o, width=15), expected)
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 873, in _safe_repr
    orepr, oreadable, orecur = self.format(
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
FAIL: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='empty', prefix='dict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
AssertionError: 'dict_view([])' != 'dict_keys([])'
- dict_view([])
?      ^^ ^
+ dict_keys([])
?      ^ ^^


======================================================================
FAIL: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='empty', prefix='odict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
AssertionError: 'dict_view([])' != 'odict_keys([])'
- dict_view([])
?      ^^ ^
+ odict_keys([])
? +     ^ ^^


======================================================================
FAIL: test_dict_views (__main__.QueryTestCase.test_dict_views) (length='empty', prefix='dict')
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 609, in test_dict_views
    self.assertEqual(pprint.pformat(k, sort_dicts=True),
AssertionError: 'dict_view([])' != 'dict_keys([])'
- dict_view([])
?      ^^ ^
+ dict_keys([])
?      ^ ^^


======================================================================
FAIL: test_ordered_dict (__main__.QueryTestCase.test_ordered_dict)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 543, in test_ordered_dict
    self.assertEqual(pprint.pformat(d.keys(), sort_dicts=False),
AssertionError: "dict_view(['the',\n 'quick',\n 'brown',\n[52 chars]g'])" != "odict_keys(['the',\n 'quick',\n 'brown',\[53 chars]g'])"
- dict_view(['the',
?      ^^ ^
+ odict_keys(['the',
? +     ^ ^^
   'quick',
   'brown',
   'fox',
   'jumped',
   'over',
   'a',
   'lazy',
   'dog'])


======================================================================
FAIL: test_same_as_repr (__main__.QueryTestCase.test_same_as_repr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 287, in test_same_as_repr
    self.assertEqual(pprint.pformat(simple), native)
AssertionError: 'dict_view([])' != 'dict_keys([])'
- dict_view([])
?      ^^ ^
+ dict_keys([])
?      ^ ^^


======================================================================
FAIL: test_set_reprs (__main__.QueryTestCase.test_set_reprs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pprint.py", line 891, in test_set_reprs
    self.assertEqual(pprint.pformat(set3(range(7)), width=20),
AssertionError: '{0, 1, 2, 3, 4, 5, 6}' != 'set3({0, 1, 2, 3, 4, 5, 6})'
- {0, 1, 2, 3, 4, 5, 6}
+ set3({0, 1, 2, 3, 4, 5, 6})
? +++++                     +


----------------------------------------------------------------------
Ran 76 tests in Ns

FAILED (failures=6, errors=42, skipped=1)
