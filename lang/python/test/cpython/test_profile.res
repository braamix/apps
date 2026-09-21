F.F.F..
======================================================================
FAIL: test_calling_conventions (__main__.ProfileTest.test_calling_conventions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_profile.py", line 90, in test_calling_conventions
    self.assertIn(self.expected_max_output, res,
AssertionError: ':0(max)' not found in '         4 function calls in -0.005 seconds\n\n   Random listing order was used\n\n   ncalls  tottime  percall  cumtime  percall filename:lineno(function)\n        0    0.000             0.000          profile:0(profiler)\n        1   -0.002   -0.002   -0.005   -0.005 profile:0(max([0], **dict(key=int)))\n        1   -0.002   -0.002   -0.003   -0.003 :0(exec)\n        1   -0.001   -0.001   -0.001   -0.001 <string>:1(<module>)\n        1    0.000    0.000    0.000    0.000 :0(setprofile)\n\n\n' : Profiling 'max([0], **dict(key=int))' didn't report max:
         4 function calls in -0.005 seconds

   Random listing order was used

   ncalls  tottime  percall  cumtime  percall filename:lineno(function)
        0    0.000             0.000          profile:0(profiler)
        1   -0.002   -0.002   -0.005   -0.005 profile:0(max([0], **dict(key=int)))
        1   -0.002   -0.002   -0.003   -0.003 :0(exec)
        1   -0.001   -0.001   -0.001   -0.001 <string>:1(<module>)
        1    0.000    0.000    0.000    0.000 :0(setprofile)




======================================================================
FAIL: test_output_file_when_changing_directory (__main__.ProfileTest.test_output_file_when_changing_directory)
----------------------------------------------------------------------
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-I', '-m', 'profile', '-o', 'out.pstats', 'demo.py']

stdout:
---

---

stderr:
---
Traceback (most recent call last):
  File "<string>", line 2, in <module>
  File "/pkg/store/python-0/lib/runpy.py", line 201, in _run_module_as_main
  File "/pkg/store/python-0/lib/runpy.py", line 87, in _run_code
  File "/pkg/store/python-0/lib/profile.py", line 623, in <module>
  File "/pkg/store/python-0/lib/profile.py", line 612, in main
  File "/pkg/store/python-0/lib/profile.py", line 109, in runctx
  File "/pkg/store/python-0/lib/profile.py", line 76, in runctx
  File "/pkg/store/python-0/lib/profile.py", line 80, in _show
  File "/pkg/store/python-0/lib/profile.py", line 405, in dump_stats
ValueError: unmarshallable object
---

======================================================================
FAIL: test_run_profile_as_module (__main__.ProfileTest.test_run_profile_as_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_profile.py", line 120, in test_run_profile_as_module
    assert_python_ok('-m', self.profilermodule.__name__,
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-I', '-m', 'profile', '-m', 'timeit', '-n', '1']

stdout:
---
         1070 function calls (1067 primitive calls) in 0.000 seconds

   Ordered by: standard name

   ncalls  tottime  percall  cumtime  percall filename:lineno(function)
    45/43    0.000    0.000    0.000    0.000 :0(__build_class__)
        1    0.000    0.000    0.000    0.000 :0(__exit__)
        5    0.000    0.000    0.000    0.000 :0(__init__)
       47    0.000    0.000    0.000    0.000 :0(__setitem__)
        3    0.000    0.000    0.000    0.000 :0(acquire_lock)
       26    0.000    0.000    0.000    0.000 :0(add)
        2    0.000    0.000    0.000    0.000 :0(any)
       11    0.000    0.000    0.000    0.000 :0(append)
        1    0.000    0.000    0.000    0.000 :0(can_colorize)
       24    0.000    0.000    0.000    0.000 :0(delattr)
        7    0.000    0.000    0.000    0.000 :0(endswith)
      2/1    0.000    0.000    0.000    0.000 :0(exec)
        3    0.000    0.000    0.000    0.000 :0(extension_suffixes)
        6    0.000    0.000    0.000    0.000 :0(fspath)
       11    0.000    0.000    0.000    0.000 :0(get)
        1    0.000    0.000    0.000    0.000 :0(get_theme)
       75    0.000    0.000    0.000    0.000 :0(getattr)
       16    0.000    0.000    0.000    0.000 :0(hasattr)
        1    0.000    0.000    0.000    0.000 :0(is_builtin)
       71    0.000    0.000    0.000    0.000 :0(isinstance)
        1    0.000    0.000    0.000    0.000 :0(issubclass)
        4    0.000    0.000    0.000    0.000 :0(items)
        1    0.000    0.000    0.000    0.000 :0(join)
      192    0.000    0.000    0.000    0.000 :0(len)
        2    0.000    0.000    0.000    0.000 :0(lower)
        2    0.000    0.000    0.000    0.000 :0(max)
        1    0.000    0.000    0.000    0.000 :0(object)
        1    0.000    0.000    0.000    0.000 :0(open_code)
       13    0.000    0.000    0.000    0.000 :0(pop)
        1    0.000    0.000    0.000    0.000 :0(read)
        3    0.000    0.000    0.000    0.000 :0(release_lock)
        2    0.000    0.000    0.000    0.000 :0(rfind)
       10    0.000    0.000    0.000    0.000 :0(rpartition)
        2    0.000    0.000    0.000    0.000 :0(rstrip)
        8    0.000    0.000    0.000    0.000 :0(setattr)
       16    0.000    0.000    0.000    0.000 :0(setdefault)
       34    0.000    0.000    0.000    0.000 :0(startswith)
        2    0.000    0.000    0.000    0.000 :0(stat)
        5    0.000    0.000    0.000    0.000 :0(update)
        5    0.000    0.000    0.000    0.000 <frozen importlib._bootstrap>:7(_spec)
        1    0.000    0.000    0.000    0.000 <string>:1(<module>)
        1    0.000    0.000    0.000    0.000 __init__.py:1(<module>)
        1    0.000    0.000    0.000    0.000 _bootstrap.py:1100(find_spec)
        3    0.000    0.000    0.000    0.000 _bootstrap.py:1174(__enter__)
        3    0.000    0.000    0.000    0.000 _bootstrap.py:1178(__exit__)
        1    0.000    0.000    0.000    0.000 _bootstrap.py:1192(_find_spec)
        2    0.000    0.000    0.000    0.000 _bootstrap.py:535(_call_with_frames_removed)
        2    0.000    0.000    0.000    0.000 _bootstrap.py:546(_verbose_message)
        2    0.000    0.000    0.000    0.000 _bootstrap.py:556(_requires_builtin_wrapper)
        6    0.000    0.000    0.000    0.000 _bootstrap.py:635(__init__)
        1    0.000    0.000    0.000    0.000 _bootstrap.py:669(cached)
        6    0.000    0.000    0.000    0.000 _bootstrap.py:682(parent)
        5    0.000    0.000    0.000    0.000 _bootstrap.py:690(has_location)
        2    0.000    0.000    0.000    0.000 _bootstrap.py:698(spec_from_loader)
        5    0.000    0.000    0.000    0.000 _bootstrap.py:765(_init_module_attrs)
        1    0.000    0.000    0.000    0.000 _bootstrap.py:956(find_spec)
        2    0.000    0.000    0.000    0.000 _bootstrap.py:989(is_package)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:1219(_path_importer_cache)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:1241(_get_spec)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:1270(find_spec)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:134(_path_join)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:1354(_get_spec)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:1359(find_spec)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:136(<listcomp>)
        2    0.000    0.000    0.000    0.000 _bootstrap_external.py:140(_path_split)
        4    0.000    0.000    0.000    0.000 _bootstrap_external.py:142(<genexpr>)
        2    0.000    0.000    0.000    0.000 _bootstrap_external.py:148(_path_stat)
        3    0.000    0.000    0.000    0.000 _bootstrap_external.py:1566(_get_supported_file_loaders)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:158(_path_is_mode_type)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:167(_path_isfile)
        4    0.000    0.000    0.000    0.000 _bootstrap_external.py:188(_path_isabs)
        4    0.000    0.000    0.000    0.000 _bootstrap_external.py:193(_path_abspath)
        2    0.000    0.000    0.000    0.000 _bootstrap_external.py:242(cache_from_source)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:361(_get_cached)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:393(_check_name_wrapper)
        4    0.000    0.000    0.000    0.000 _bootstrap_external.py:551(spec_from_file_location)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:74(_relax_case)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:808(source_to_code)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:817(get_code)
        4    0.000    0.000    0.000    0.000 _bootstrap_external.py:908(__init__)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:922(get_filename)
        1    0.000    0.000    0.000    0.000 _bootstrap_external.py:926(get_data)
        1    0.000    0.000    0.000    0.000 argparse.py:1(<module>)
        1    0.000    0.000    0.000    0.000 argparse.py:1046(BooleanOptionalAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1096(_StoreAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1133(_StoreConstAction)
        1    0.000    0.000    0.000    0.000 argparse.py:115(_AttributeHolder)
        1    0.000    0.000    0.000    0.000 argparse.py:1158(_StoreTrueAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1177(_StoreFalseAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1196(_AppendAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1236(_AppendConstAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1265(_CountAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1290(_HelpAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1311(_VersionAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1340(_SubParsersAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1342(_ChoicesPseudoAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1458(_ExtendAction)
        1    0.000    0.000    0.000    0.000 argparse.py:1469(FileType)
        1    0.000    0.000    0.000    0.000 argparse.py:1529(Namespace)
        1    0.000    0.000    0.000    0.000 argparse.py:1555(_ActionsContainer)
        1    0.000    0.000    0.000    0.000 argparse.py:1557(__init__)
       12    0.000    0.000    0.000    0.000 argparse.py:1611(register)
        1    0.000    0.000    0.000    0.000 argparse.py:164(_ColorlessTheme)
        1    0.000    0.000    0.000    0.000 argparse.py:176(HelpFormatter)
        1    0.000    0.000    0.000    0.000 argparse.py:1840(_get_handler)
        1    0.000    0.000    0.000    0.000 argparse.py:1895(_ArgumentGroup)
        1    0.000    0.000    0.000    0.000 argparse.py:1938(_MutuallyExclusiveGroup)
        1    0.000    0.000    0.000    0.000 argparse.py:1985(ArgumentParser)
        1    0.000    0.000    0.000    0.000 argparse.py:2010(__init__)
        1    0.000    0.000    0.000    0.000 argparse.py:254(_Section)
        1    0.000    0.000    0.000    0.000 argparse.py:824(RawDescriptionHelpFormatter)
        1    0.000    0.000    0.000    0.000 argparse.py:835(RawTextHelpFormatter)
        1    0.000    0.000    0.000    0.000 argparse.py:846(ArgumentDefaultsHelpFormatter)
        1    0.000    0.000    0.000    0.000 argparse.py:877(MetavarTypeHelpFormatter)
        1    0.000    0.000    0.000    0.000 argparse.py:919(ArgumentError)
        1    0.000    0.000    0.000    0.000 argparse.py:939(ArgumentTypeError)
        1    0.000    0.000    0.000    0.000 argparse.py:948(Action)
        1    0.000    0.000    0.000    0.000 enum.py:1(<module>)
        7    0.000    0.000    0.000    0.000 enum.py:1005(_find_data_type_)
        4    0.000    0.000    0.000    0.000 enum.py:1032(_find_new_)
        1    0.000    0.000    0.000    0.000 enum.py:1141(Enum)
        1    0.000    0.000    0.000    0.000 enum.py:1366(ReprEnum)
        1    0.000    0.000    0.000    0.000 enum.py:1372(IntEnum)
        1    0.000    0.000    0.000    0.000 enum.py:1378(StrEnum)
        2    0.000    0.000    0.000    0.000 enum.py:1405(_generate_next_value_)
        1    0.000    0.000    0.000    0.000 enum.py:1421(FlagBoundary)
        1    0.000    0.000    0.000    0.000 enum.py:155(_not_given)
        1    0.000    0.000    0.000    0.000 enum.py:160(_auto_null)
        1    0.000    0.000    0.000    0.000 enum.py:165(auto)
        3    0.000    0.000    0.000    0.000 enum.py:169(__init__)
        1    0.000    0.000    0.000    0.000 enum.py:175(property)
        4    0.000    0.000    0.000    0.000 enum.py:227(__set_name__)
        1    0.000    0.000    0.000    0.000 enum.py:23(nonmember)
        1    0.000    0.000    0.000    0.000 enum.py:232(_proto_member)
        1    0.000    0.000    0.000    0.000 enum.py:30(member)
        1    0.000    0.000    0.000    0.000 enum.py:328(EnumDict)
        5    0.000    0.000    0.000    0.000 enum.py:335(__init__)
       48    0.000    0.000    0.000    0.000 enum.py:343(__setitem__)
        5    0.000    0.000    0.000    0.000 enum.py:37(_is_descriptor)
        2    0.000    0.000    0.000    0.000 enum.py:415(<genexpr>)
        1    0.000    0.000    0.000    0.000 enum.py:462(EnumType)
        5    0.000    0.000    0.000    0.000 enum.py:468(__prepare__)
       39    0.000    0.000    0.000    0.000 enum.py:47(_is_dunder)
        4    0.000    0.000    0.000    0.000 enum.py:481(__new__)
       48    0.000    0.000    0.000    0.000 enum.py:58(_is_sunder)
        3    0.000    0.000    0.000    0.000 enum.py:69(_is_internal_class)
       48    0.000    0.000    0.000    0.000 enum.py:78(_is_private)
        5    0.000    0.000    0.000    0.000 enum.py:954(_check_for_existing_members_)
        9    0.000    0.000    0.000    0.000 enum.py:964(_get_mixins_)
        4    0.000    0.000    0.000    0.000 enum.py:983(_find_data_repr_)
        0    0.000             0.000          profile:0(profiler)
        1    0.000    0.000    0.000    0.000 profile:0(run_module(modname, run_name='__main__'))
        1    0.000    0.000    0.000    0.000 runpy.py:104(_get_module_details)
        1    0.000    0.000    0.000    0.000 runpy.py:204(run_module)
        1    0.000    0.000    0.000    0.000 runpy.py:65(_run_code)
        1    0.000    0.000    0.000    0.000 timeit.py:1(<module>)
        1    0.000    0.000    0.000    0.000 timeit.py:222(main)
        1    0.000    0.000    0.000    0.000 timeit.py:55(Timer)
        4    0.000    0.000    0.000    0.000 types.py:213(__init__)
        1    0.000    0.000    0.000    0.000 util.py:73(find_spec)
---

stderr:
---
Traceback (most recent call last):
  File "<string>", line 2, in <module>
  File "/pkg/store/python-0/lib/runpy.py", line 201, in _run_module_as_main
  File "/pkg/store/python-0/lib/runpy.py", line 87, in _run_code
  File "/pkg/store/python-0/lib/profile.py", line 623, in <module>
  File "/pkg/store/python-0/lib/profile.py", line 612, in main
  File "/pkg/store/python-0/lib/profile.py", line 109, in runctx
  File "/pkg/store/python-0/lib/profile.py", line 72, in runctx
  File "/pkg/store/python-0/lib/profile.py", line 432, in runctx
  File "/pkg/store/python-0/lib/profile.py", line 217, in trace_dispatch_i
  File "/pkg/store/python-0/lib/profile.py", line 301, in trace_dispatch_return
AssertionError: ('Bad return', ('/pkg/store/python-0/lib/enum.py', 343, '__setitem__'))
---

----------------------------------------------------------------------
Ran 7 tests in Ns

FAILED (failures=3)
