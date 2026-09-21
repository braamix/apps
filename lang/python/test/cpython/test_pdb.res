...EEEEEEEEEEE.FFFFEE.EEEEEEEEEEEE.EEEEEEEEEEEEE.F...EEEEEEE.sEEEEssssEEEEEssssssssssssEE.F.FEF.F.E.FEE.E.FEE.E.F..F.E.F..FEE.F.F.F.E.FEF.F.F.E.F.F.F.F.F.E.F.F.E.E.F.EE.EEEE.E.E.F.F.EEEE.FE.FF.F.F.F.EEEE.F.F.F.FEE.E.E.E.E.E..E.F.F.F.F.E.F.E.F.F.F.E.F..F.F.F..F.E.F.F.F.E.F.F.F.F.E.F.F.F.E.F.F.F.F.E.E.F.EE.EE.E.F.F.F.F.EE.E.F..FF.F.F.F.EE.F.F.F.F.E.E.E.E.E.E.E..
======================================================================
ERROR: test_async_break (__main__.PdbTestCase.test_async_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4757, in test_async_break
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_blocks_at_first_code_line (__main__.PdbTestCase.test_blocks_at_first_code_line)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4257, in test_blocks_at_first_code_line
    stdout, stderr = self.run_pdb_module(script, commands)
  File "/tmp/test_pdb.py", line 3597, in run_pdb_module
    return self._run_pdb(['-m', self.module_name], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_breakpoint (__main__.PdbTestCase.test_breakpoint)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4188, in test_breakpoint
    stdout, stderr = self.run_pdb_module(script, commands)
  File "/tmp/test_pdb.py", line 3597, in run_pdb_module
    return self._run_pdb(['-m', self.module_name], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_dir_as_script (__main__.PdbTestCase.test_dir_as_script)
----------------------------------------------------------------------
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_empty_file (__main__.PdbTestCase.test_empty_file)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4655, in test_empty_file
    stdout, _ = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_end_of_options_separator (__main__.PdbTestCase.test_end_of_options_separator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4726, in test_end_of_options_separator
    stdout, _ = self._run_pdb(['--', os_helper.TESTFN, '-foo'], 'c\nq')
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_errors_in_command (__main__.PdbTestCase.test_errors_in_command)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4457, in test_errors_in_command
    stdout, _ = self.run_pdb_script('pass', commands + '\n')
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_file_modified_after_execution (__main__.PdbTestCase.test_file_modified_after_execution)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4276, in test_file_modified_after_execution
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_file_modified_after_execution_with_multiple_instances (__main__.PdbTestCase.test_file_modified_after_execution_with_multiple_instances)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4325, in test_file_modified_after_execution_with_multiple_instances
    env = {**os.environ, 'PYTHONIOENCODING': 'utf-8'},
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_file_modified_after_execution_with_restart (__main__.PdbTestCase.test_file_modified_after_execution_with_restart)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4357, in test_file_modified_after_execution_with_restart
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_file_modified_and_immediately_restarted (__main__.PdbTestCase.test_file_modified_and_immediately_restarted)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4295, in test_file_modified_and_immediately_restarted
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_gh_93696_frozen_list (__main__.PdbTestCase.test_gh_93696_frozen_list)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4645, in test_gh_93696_frozen_list
    stdout, _ = self._run_pdb(["gh93696_host.py"], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_gh_94215_crash (__main__.PdbTestCase.test_gh_94215_crash)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4587, in test_gh_94215_crash
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_invalid_cmd_line_options (__main__.PdbTestCase.test_invalid_cmd_line_options)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4243, in test_invalid_cmd_line_options
    stdout, stderr = self._run_pdb(["-c"], "", expected_returncode=2)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue13120 (__main__.PdbTestCase.test_issue13120)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3868, in test_issue13120
    env={**os.environ, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue13183 (__main__.PdbTestCase.test_issue13183)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3842, in test_issue13183
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue16180 (__main__.PdbTestCase.test_issue16180)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3910, in test_issue16180
    stdout, stderr = self.run_pdb_script(
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue26053 (__main__.PdbTestCase.test_issue26053)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3943, in test_issue26053
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue34266 (__main__.PdbTestCase.test_issue34266)
do_run handles exceptions from parsing its arg
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4488, in test_issue34266
    check('\\', 'No escaped character')
  File "/tmp/test_pdb.py", line 4482, in check
    stdout, _ = self.run_pdb_script('pass', commands + '\n')
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue36250 (__main__.PdbTestCase.test_issue36250)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3898, in test_issue36250
    env = {**os.environ, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue42383 (__main__.PdbTestCase.test_issue42383)
----------------------------------------------------------------------
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue42384 (__main__.PdbTestCase.test_issue42384)
When running `python foo.py` sys.path[0] is an absolute path. `python -m pdb foo.py` should behave the same
----------------------------------------------------------------------
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue42384_symlink (__main__.PdbTestCase.test_issue42384_symlink)
When running `python foo.py` sys.path[0] resolves symlinks. `python -m pdb foo.py` should behave the same
----------------------------------------------------------------------
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue46434 (__main__.PdbTestCase.test_issue46434)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3804, in test_issue46434
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue58956 (__main__.PdbTestCase.test_issue58956)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3973, in test_issue58956
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue84583 (__main__.PdbTestCase.test_issue84583)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3926, in test_issue84583
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_issue_59000 (__main__.PdbTestCase.test_issue_59000)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4774, in test_issue_59000
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_module_is_run_as_main (__main__.PdbTestCase.test_module_is_run_as_main)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4146, in test_module_is_run_as_main
    stdout, stderr = self.run_pdb_module(script, commands)
  File "/tmp/test_pdb.py", line 3597, in run_pdb_module
    return self._run_pdb(['-m', self.module_name], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_module_without_a_main (__main__.PdbTestCase.test_module_without_a_main)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4211, in test_module_without_a_main
    stdout, stderr = self._run_pdb(
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_non_utf8_encoding (__main__.PdbTestCase.test_non_utf8_encoding)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4662, in test_non_utf8_encoding
    for filename in os.listdir(script_dir):
FileNotFoundError: [Errno 2] No such file or directory: '/tmp/encoded_modules'

======================================================================
ERROR: test_nonexistent_module (__main__.PdbTestCase.test_nonexistent_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4234, in test_nonexistent_module
    stdout, stderr = self._run_pdb(["-m", os_helper.TESTFN], "", expected_returncode=1)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_package_without_a_main (__main__.PdbTestCase.test_package_without_a_main)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4225, in test_package_without_a_main
    stdout, stderr = self._run_pdb(
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_pdbrc_alias (__main__.PdbTestCase.test_pdbrc_alias)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4042, in test_pdbrc_alias
    stdout, stderr = self.run_pdb_script(script, 'q\n', pdbrc=pdbrc, remove_home=True)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_pdbrc_basic (__main__.PdbTestCase.test_pdbrc_basic)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4004, in test_pdbrc_basic
    stdout, stderr = self.run_pdb_script(script, 'q\n', pdbrc=pdbrc, remove_home=True)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_pdbrc_commands (__main__.PdbTestCase.test_pdbrc_commands)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4076, in test_pdbrc_commands
    stdout, stderr = self.run_pdb_script(script, 'q\n', pdbrc=pdbrc, remove_home=True)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_pdbrc_empty_line (__main__.PdbTestCase.test_pdbrc_empty_line)
Test that empty lines in .pdbrc are ignored.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4023, in test_pdbrc_empty_line
    stdout, stderr = self.run_pdb_script(script, 'q\n', pdbrc=pdbrc, remove_home=True)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_pdbrc_semicolon (__main__.PdbTestCase.test_pdbrc_semicolon)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4058, in test_pdbrc_semicolon
    stdout, stderr = self.run_pdb_script(script, 'q\n', pdbrc=pdbrc, remove_home=True)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_post_mortem_restart (__main__.PdbTestCase.test_post_mortem_restart)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4379, in test_post_mortem_restart
    stdout, stderr = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_readrc_kwarg (__main__.PdbTestCase.test_readrc_kwarg)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4084, in test_readrc_kwarg
    stdout, stderr = self.run_pdb_script(script, 'q\n', pdbrc='invalid', remove_home=True)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_relative_imports (__main__.PdbTestCase.test_relative_imports)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4414, in test_relative_imports
    stdout, _ = self._run_pdb(['-m', self.module_name], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_relative_imports_on_plain_module (__main__.PdbTestCase.test_relative_imports_on_plain_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4447, in test_relative_imports_on_plain_module
    stdout, _ = self._run_pdb(['-m', self.module_name + '.runme'], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_run_module (__main__.PdbTestCase.test_run_module)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4134, in test_run_module
    stdout, stderr = self.run_pdb_module(script, commands)
  File "/tmp/test_pdb.py", line 3597, in run_pdb_module
    return self._run_pdb(['-m', self.module_name], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_run_module_with_args (__main__.PdbTestCase.test_run_module_with_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4153, in test_run_module_with_args
    self._run_pdb(["calendar", "-m"], commands, expected_returncode=1)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_run_pdb_with_pdb (__main__.PdbTestCase.test_run_pdb_with_pdb)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4197, in test_run_pdb_with_pdb
    stdout, stderr = self._run_pdb(["-m", "pdb"], commands)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_run_script_with_args (__main__.PdbTestCase.test_run_script_with_args)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4174, in test_run_script_with_args
    stdout, stderr = self.run_pdb_script(script, commands, script_args=["--bar", "foo"])
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_spec (__main__.PdbTestCase.test_spec)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3706, in test_spec
    stdout, _ = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_step_into_botframe (__main__.PdbTestCase.test_step_into_botframe)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3988, in test_step_into_botframe
    stdout, _ = self.run_pdb_script(script, commands)
  File "/tmp/test_pdb.py", line 3582, in run_pdb_script
    stdout, stderr = self._run_pdb([filename] + script_args, commands, expected_returncode, extra_env)
  File "/tmp/test_pdb.py", line 3548, in _run_pdb
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_zipapp (__main__.PdbTestCase.test_zipapp)
----------------------------------------------------------------------
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_zipimport (__main__.PdbTestCase.test_zipimport)
----------------------------------------------------------------------
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_alternate_stdin (__main__.PdbTestInline.test_alternate_stdin)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4967, in test_alternate_stdin
    stdout, stderr = self._run_script(script, commands)
  File "/tmp/test_pdb.py", line 4850, in _run_script
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_quit (__main__.PdbTestInline.test_quit)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4876, in test_quit
    stdout, stderr = self._run_script(script, commands, expected_returncode=1)
  File "/tmp/test_pdb.py", line 4850, in _run_script
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_quit_after_interact (__main__.PdbTestInline.test_quit_after_interact)
interact command will set sys.ps1 temporarily, we need to make sure
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4906, in test_quit_after_interact
    stdout, stderr = self._run_script(script, commands, expected_returncode=1)
  File "/tmp/test_pdb.py", line 4850, in _run_script
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_readline_not_imported (__main__.PdbTestInline.test_readline_not_imported)
GH-138860
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4954, in test_readline_not_imported
    stdout, stderr = self._run_script(script, commands)
  File "/tmp/test_pdb.py", line 4850, in _run_script
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_set_trace_with_skip (__main__.PdbTestInline.test_set_trace_with_skip)
GH-82897
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4933, in test_set_trace_with_skip
    stdout, _ = self._run_script(script, commands)
  File "/tmp/test_pdb.py", line 4850, in _run_script
    env = {**env, 'PYTHONIOENCODING': 'utf-8'}
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_return_from_inline_mode_to_REPL (__main__.TestREPLSession.test_return_from_inline_mode_to_REPL)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 5045, in test_return_from_inline_mode_to_REPL
    from test.test_repl import spawn_repl
ModuleNotFoundError: No module named 'test.test_repl'. Did you mean: 'test.test_email'?

======================================================================
ERROR: test_convenience_variables (__main__) [2]
Doctest: __main__.test_convenience_variables
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1193, in __main__.test_convenience_variables
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_convenience_variables[2]>", line 24, in <module>
    test_function()
  File "<doctest __main__.test_convenience_variables[1]>", line 2, in test_function
    util_function()
  File "<doctest __main__.test_convenience_variables[0]>", line 3, in util_function
    try:
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 150, in exception_callback
    frame.f_trace(frame, 'exception', (type(exc), exc, exc.__traceback__))
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_next_until_return_at_return_event (__main__)
Doctest: __main__.test_next_until_return_at_return_event
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_basic_commands (__main__) [4]
Doctest: __main__.test_pdb_basic_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 105, in __main__.test_pdb_basic_commands
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_basic_commands[4]>", line 26, in <module>
    test_function()
  File "<doctest __main__.test_pdb_basic_commands[3]>", line 3, in test_function
    ret = test_function_2('baz')
  File "<doctest __main__.test_pdb_basic_commands[0]>", line 4, in test_function_2
    print(i)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_break_anywhere (__main__)
Doctest: __main__.test_pdb_break_anywhere
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_breakpoint_commands (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 245, in __main__.test_pdb_breakpoint_commands
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_breakpoint_commands[1]>", line 1, in <module>
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
TypeError: __repr__ returned a non-string: _rstr

======================================================================
ERROR: test_pdb_breakpoint_ignore_and_condition (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_ignore_and_condition
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 370, in __main__.test_pdb_breakpoint_ignore_and_condition
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_breakpoint_ignore_and_condition[1]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_pdb_breakpoint_ignore_and_condition[0]>", line 4, in test_function
    print(i)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_breakpoint_on_annotated_function_def (__main__)
Doctest: __main__.test_pdb_breakpoint_on_annotated_function_def
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_breakpoint_on_disabled_line (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_on_disabled_line
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 526, in __main__.test_pdb_breakpoint_on_disabled_line
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_breakpoint_on_disabled_line[1]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>", line 5, in test_function
    print(j)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_breakpoint_with_filename (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_with_filename
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 494, in __main__.test_pdb_breakpoint_with_filename
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE +ELLIPSIS
  File "<doctest __main__.test_pdb_breakpoint_with_filename[1]>", line 8, in <module>
    test_function()
  File "<doctest __main__.test_pdb_breakpoint_with_filename[0]>", line 3, in test_function
    from test.test_inspect import inspect_fodder2 as mod2
ModuleNotFoundError: No module named 'test.test_inspect'. Did you mean: 'test.test_doctest'?

======================================================================
ERROR: test_pdb_commands (__main__) [1]
Doctest: __main__.test_pdb_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 447, in __main__.test_pdb_commands
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_commands[1]>", line 14, in <module>
    test_function()
  File "<doctest __main__.test_pdb_commands[0]>", line 4, in test_function
    print(2)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 130, in line_callback
    frame.f_trace(frame, 'line', None)
IndexError: pop from empty list

======================================================================
ERROR: test_pdb_continue_in_bottomframe (__main__)
Doctest: __main__.test_pdb_continue_in_bottomframe
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_display_command (__main__) [1]
Doctest: __main__.test_pdb_display_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 875, in __main__.test_pdb_display_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
  File "<doctest __main__.test_pdb_display_command[1]>", line 17, in <module>
    test_function()
  File "<doctest __main__.test_pdb_display_command[0]>", line 3, in test_function
    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 130, in line_callback
    frame.f_trace(frame, 'line', None)
TypeError: __repr__ returned a non-string: _rstr

======================================================================
ERROR: test_pdb_frame_refleak (__main__) [2]
Doctest: __main__.test_pdb_frame_refleak
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3361, in __main__.test_pdb_frame_refleak
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_frame_refleak[2]>", line 8, in <module>
    test_function()
  File "<doctest __main__.test_pdb_frame_refleak[1]>", line 5, in test_function
    print(len(gc.get_referrers(container[0])))
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_pdb_function_break (__main__)
Doctest: __main__.test_pdb_function_break
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_issue_43318 (__main__) [1]
Doctest: __main__.test_pdb_issue_43318
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2979, in __main__.test_pdb_issue_43318
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_43318[1]>", line 6, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_43318[0]>", line 2, in test_function
    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 130, in line_callback
    frame.f_trace(frame, 'line', None)
IndexError: pop from empty list

======================================================================
ERROR: test_pdb_issue_gh_136057 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_136057
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3244, in __main__.test_pdb_issue_gh_136057
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_gh_136057[1]>", line 7, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_gh_136057[0]>", line 4, in test_function
    for i in lst: pass
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_issue_gh_91742 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_91742
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3015, in __main__.test_pdb_issue_gh_91742
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_gh_91742[1]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_gh_91742[0]>", line 12, in test_function
    about()
  File "<doctest __main__.test_pdb_issue_gh_91742[0]>", line 6, in about
    '''About'''
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 130, in line_callback
    frame.f_trace(frame, 'line', None)
Exception

======================================================================
ERROR: test_pdb_issue_gh_94215 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_94215
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3061, in __main__.test_pdb_issue_gh_94215
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_gh_94215[1]>", line 15, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_gh_94215[0]>", line 9, in test_function
    func()
  File "<doctest __main__.test_pdb_issue_gh_94215[0]>", line 3, in func
    def inner(v): pass
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 130, in line_callback
    frame.f_trace(frame, 'line', None)
Exception

======================================================================
ERROR: test_pdb_next_command_for_asyncgen (__main__) [0]
Doctest: __main__.test_pdb_next_command_for_asyncgen
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2382, in __main__.test_pdb_next_command_for_asyncgen
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_next_command_for_asyncgen (__main__) [5]
Doctest: __main__.test_pdb_next_command_for_asyncgen
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2401, in __main__.test_pdb_next_command_for_asyncgen
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[5]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[4]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 196, in run_yielding_async_fn
    coro.send(None)
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[3]>", line 3, in test_main
    await test_coro()
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[2]>", line 3, in test_coro
    print(x)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_next_command_for_coroutine (__main__) [0]
Doctest: __main__.test_pdb_next_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2325, in __main__.test_pdb_next_command_for_coroutine
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_next_command_for_coroutine[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_next_command_for_coroutine (__main__) [4]
Doctest: __main__.test_pdb_next_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2340, in __main__.test_pdb_next_command_for_coroutine
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_next_command_for_coroutine[4]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_for_coroutine[3]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 196, in run_yielding_async_fn
    coro.send(None)
  File "<doctest __main__.test_pdb_next_command_for_coroutine[2]>", line 3, in test_main
    await test_coro()
  File "<doctest __main__.test_pdb_next_command_for_coroutine[1]>", line 2, in test_coro
    await async_yield(0)
NameError: name 'async_yield' is not defined

======================================================================
ERROR: test_pdb_next_command_for_coroutine (__main__)
Doctest: __main__.test_pdb_next_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_next_command_for_generator (__main__) [2]
Doctest: __main__.test_pdb_next_command_for_generator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2081, in __main__.test_pdb_next_command_for_generator
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_next_command_for_generator[2]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_for_generator[1]>", line 4, in test_function
    try:
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 150, in exception_callback
    frame.f_trace(frame, 'exception', (type(exc), exc, exc.__traceback__))
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_next_command_in_generator_for_loop (__main__) [2]
Doctest: __main__.test_pdb_next_command_in_generator_for_loop
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2654, in __main__.test_pdb_next_command_in_generator_for_loop
    >>> with PdbTestInput(['break test_gen',
  File "<doctest __main__.test_pdb_next_command_in_generator_for_loop[2]>", line 7, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_in_generator_for_loop[1]>", line 4, in test_function
    print('value', i)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_next_command_subiterator (__main__) [3]
Doctest: __main__.test_pdb_next_command_subiterator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2699, in __main__.test_pdb_next_command_subiterator
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_next_command_subiterator[3]>", line 8, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_subiterator[2]>", line 4, in test_function
    print('value', i)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_return_command_for_coroutine (__main__) [0]
Doctest: __main__.test_pdb_return_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2504, in __main__.test_pdb_return_command_for_coroutine
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_return_command_for_coroutine[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_return_command_for_coroutine (__main__) [4]
Doctest: __main__.test_pdb_return_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2519, in __main__.test_pdb_return_command_for_coroutine
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_return_command_for_coroutine[4]>", line 6, in <module>
    test_function()
  File "<doctest __main__.test_pdb_return_command_for_coroutine[3]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 196, in run_yielding_async_fn
    coro.send(None)
  File "<doctest __main__.test_pdb_return_command_for_coroutine[2]>", line 3, in test_main
    await test_coro()
  File "<doctest __main__.test_pdb_return_command_for_coroutine[1]>", line 2, in test_coro
    await async_yield(0)
NameError: name 'async_yield' is not defined

======================================================================
ERROR: test_pdb_return_command_for_coroutine (__main__)
Doctest: __main__.test_pdb_return_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_return_command_for_generator (__main__) [2]
Doctest: __main__.test_pdb_return_command_for_generator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2462, in __main__.test_pdb_return_command_for_generator
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_return_command_for_generator[2]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_return_command_for_generator[1]>", line 4, in test_function
    try:
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 150, in exception_callback
    frame.f_trace(frame, 'exception', (type(exc), exc, exc.__traceback__))
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_return_to_different_file (__main__)
Doctest: __main__.test_pdb_return_to_different_file
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_until_command_for_coroutine (__main__) [0]
Doctest: __main__.test_pdb_until_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2597, in __main__.test_pdb_until_command_for_coroutine
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_until_command_for_coroutine[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_until_command_for_coroutine (__main__) [4]
Doctest: __main__.test_pdb_until_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2616, in __main__.test_pdb_until_command_for_coroutine
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_until_command_for_coroutine[4]>", line 5, in <module>
    test_function()
  File "<doctest __main__.test_pdb_until_command_for_coroutine[3]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 196, in run_yielding_async_fn
    coro.send(None)
  File "<doctest __main__.test_pdb_until_command_for_coroutine[2]>", line 3, in test_main
    await test_coro()
  File "<doctest __main__.test_pdb_until_command_for_coroutine[1]>", line 3, in test_coro
    await async_yield(0)
NameError: name 'async_yield' is not defined

======================================================================
ERROR: test_pdb_until_command_for_coroutine (__main__)
Doctest: __main__.test_pdb_until_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_until_command_for_generator (__main__) [2]
Doctest: __main__.test_pdb_until_command_for_generator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2559, in __main__.test_pdb_until_command_for_generator
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_until_command_for_generator[2]>", line 7, in <module>
    test_function()
  File "<doctest __main__.test_pdb_until_command_for_generator[1]>", line 4, in test_function
    print(i)
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem (__main__)
Doctest: __main__.test_post_mortem
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/pkg/store/python-0/lib/doctest.py", line 2428, in runTest
    results = runner.run(test, out=out, clear_globs=False)
  File "/pkg/store/python-0/lib/doctest.py", line 1604, in run
    return self.__run(test, compileflags, out)
  File "/pkg/store/python-0/lib/doctest.py", line 1516, in __run
    break
  File "/pkg/store/python-0/lib/bdb.py", line 94, in wrapper
    ret = func(frame, *args)
  File "/pkg/store/python-0/lib/bdb.py", line 135, in jump_callback
    inst_lineno = self._get_lineno(code, inst_offset)
  File "/pkg/store/python-0/lib/bdb.py", line 173, in _get_lineno
    for start, lineno in dis.findlinestarts(code):
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_cause_no_context (__main__) [3]
Doctest: __main__.test_post_mortem_cause_no_context
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_cause_no_context[0]>", line 3, in make_exc_with_stack
    raise type_(*content) from from_
TypeError: The Cause

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_cause_no_context[2]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_cause_no_context[1]>", line 5, in main
    raise ValueError("With Cause") from make_exc_with_stack(TypeError,'The Cause')
ValueError: With Cause

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1376, in __main__.test_post_mortem_cause_no_context
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_cause_no_context[3]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_cause_no_context[2]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_chained (__main__) [3]
Doctest: __main__.test_post_mortem_chained
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_chained[1]>", line 3, in test_function_reraise
    test_function_2()
  File "<doctest __main__.test_post_mortem_chained[0]>", line 3, in test_function_2
    1/0
ZeroDivisionError: division by zero

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_chained[2]>", line 5, in test_function
    test_function_reraise()
  File "<doctest __main__.test_post_mortem_chained[1]>", line 5, in test_function_reraise
    raise ZeroDivisionError('reraised') from e
ZeroDivisionError: reraised

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1294, in __main__.test_post_mortem_chained
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_chained[3]>", line 17, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_chained[2]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_complex (__main__) [7]
Doctest: __main__.test_post_mortem_complex
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[0]>", line 3, in make_ex_with_stack
    raise type_(*content) from from_
ValueError: Cycle2

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[0]>", line 3, in make_ex_with_stack
    raise type_(*content) from from_
ValueError: Cycle1

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[5]>", line 6, in main
    tri_cycle()
  File "<doctest __main__.test_post_mortem_complex[2]>", line 9, in tri_cycle
    raise c from a
  File "<doctest __main__.test_post_mortem_complex[0]>", line 3, in make_ex_with_stack
    raise type_(*content) from from_
ValueError: Cycle3

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[6]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_complex[5]>", line 9, in main
    raise ValueError("With Context and With Cause") from ex
ValueError: With Context and With Cause

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1643, in __main__.test_post_mortem_complex
    >>> with PdbTestInput(  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_complex[7]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_complex[6]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_context_of_the_cause (__main__) [2]
Doctest: __main__.test_post_mortem_context_of_the_cause
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_context_of_the_cause[0]>", line 3, in main
    raise TypeError('Context of the cause')
TypeError: Context of the cause

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_context_of_the_cause[0]>", line 6, in main
    raise ValueError('Root Cause')
ValueError: Root Cause

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_context_of_the_cause[1]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_context_of_the_cause[0]>", line 9, in main
    raise ValueError("With Cause, and cause has context") from ex
ValueError: With Cause, and cause has context

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1430, in __main__.test_post_mortem_context_of_the_cause
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_context_of_the_cause[2]>", line 15, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_context_of_the_cause[1]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_from_no_stack (__main__) [2]
Doctest: __main__.test_post_mortem_from_no_stack
----------------------------------------------------------------------
Exception

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_from_no_stack[1]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_from_no_stack[0]>", line 2, in main
    raise Exception() from Exception()
Exception

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1536, in __main__.test_post_mortem_from_no_stack
    >>> with PdbTestInput(  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_from_no_stack[2]>", line 7, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_from_no_stack[1]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_from_none (__main__) [2]
Doctest: __main__.test_post_mortem_from_none
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_from_none[1]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_from_none[0]>", line 5, in main
    raise ValueError("With Cause, and cause has context") from None
ValueError: With Cause, and cause has context

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1503, in __main__.test_post_mortem_from_none
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_from_none[2]>", line 6, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_from_none[1]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_convenience_variables (__main__) [2]
Doctest: __main__.test_convenience_variables
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1193, in __main__.test_convenience_variables
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_convenience_variables[2]>", line 24, in <module>
    test_function()
  File "<doctest __main__.test_convenience_variables[1]>", line 2, in test_function
    util_function()
  File "<doctest __main__.test_convenience_variables[0]>", line 3, in util_function
    try:
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_basic_commands (__main__) [4]
Doctest: __main__.test_pdb_basic_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 105, in __main__.test_pdb_basic_commands
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_basic_commands[4]>", line 26, in <module>
    test_function()
  File "<doctest __main__.test_pdb_basic_commands[3]>", line 3, in test_function
    ret = test_function_2('baz')
  File "<doctest __main__.test_pdb_basic_commands[0]>", line 5, in test_function_2
    print(bar)
Exception

======================================================================
ERROR: test_pdb_breakpoint_commands (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 245, in __main__.test_pdb_breakpoint_commands
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_breakpoint_commands[1]>", line 1, in <module>
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
TypeError: __repr__ returned a non-string: _rstr

======================================================================
ERROR: test_pdb_breakpoint_with_filename (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_with_filename
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 494, in __main__.test_pdb_breakpoint_with_filename
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE +ELLIPSIS
  File "<doctest __main__.test_pdb_breakpoint_with_filename[1]>", line 8, in <module>
    test_function()
  File "<doctest __main__.test_pdb_breakpoint_with_filename[0]>", line 3, in test_function
    from test.test_inspect import inspect_fodder2 as mod2
ModuleNotFoundError: No module named 'test.test_inspect'. Did you mean: 'test.test_doctest'?

======================================================================
ERROR: test_pdb_display_command (__main__) [1]
Doctest: __main__.test_pdb_display_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 875, in __main__.test_pdb_display_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
  File "<doctest __main__.test_pdb_display_command[1]>", line 17, in <module>
    test_function()
  File "<doctest __main__.test_pdb_display_command[0]>", line 3, in test_function
    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
TypeError: __repr__ returned a non-string: _rstr

======================================================================
ERROR: test_pdb_frame_refleak (__main__) [2]
Doctest: __main__.test_pdb_frame_refleak
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3361, in __main__.test_pdb_frame_refleak
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_frame_refleak[2]>", line 8, in <module>
    test_function()
  File "<doctest __main__.test_pdb_frame_refleak[1]>", line 5, in test_function
    print(len(gc.get_referrers(container[0])))
AttributeError: module 'gc' has no attribute 'get_referrers'

======================================================================
ERROR: test_pdb_issue_43318 (__main__) [1]
Doctest: __main__.test_pdb_issue_43318
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2979, in __main__.test_pdb_issue_43318
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_43318[1]>", line 6, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_43318[0]>", line 2, in test_function
    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
IndexError: pop from empty list

======================================================================
ERROR: test_pdb_issue_gh_108976 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_108976
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3210, in __main__.test_pdb_issue_gh_108976
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_gh_108976[1]>", line 4, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_gh_108976[0]>", line 4, in test_function
    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
IndexError: pop from empty list

======================================================================
ERROR: test_pdb_issue_gh_91742 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_91742
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3015, in __main__.test_pdb_issue_gh_91742
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_gh_91742[1]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_gh_91742[0]>", line 12, in test_function
    about()
  File "<doctest __main__.test_pdb_issue_gh_91742[0]>", line 6, in about
    '''About'''
Exception

======================================================================
ERROR: test_pdb_issue_gh_94215 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_94215
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3061, in __main__.test_pdb_issue_gh_94215
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_pdb_issue_gh_94215[1]>", line 15, in <module>
    test_function()
  File "<doctest __main__.test_pdb_issue_gh_94215[0]>", line 9, in test_function
    func()
  File "<doctest __main__.test_pdb_issue_gh_94215[0]>", line 3, in func
    def inner(v): pass
Exception

======================================================================
ERROR: test_pdb_next_command_for_asyncgen (__main__) [0]
Doctest: __main__.test_pdb_next_command_for_asyncgen
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2382, in __main__.test_pdb_next_command_for_asyncgen
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_next_command_for_asyncgen (__main__) [5]
Doctest: __main__.test_pdb_next_command_for_asyncgen
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2401, in __main__.test_pdb_next_command_for_asyncgen
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[5]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[4]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 196, in run_yielding_async_fn
    coro.send(None)
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[3]>", line 3, in test_main
    await test_coro()
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[2]>", line 2, in test_coro
    async for x in agen():
  File "<doctest __main__.test_pdb_next_command_for_asyncgen[1]>", line 3, in agen
    await async_yield(0)
NameError: name 'async_yield' is not defined

======================================================================
ERROR: test_pdb_next_command_for_coroutine (__main__) [0]
Doctest: __main__.test_pdb_next_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2325, in __main__.test_pdb_next_command_for_coroutine
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_next_command_for_coroutine[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_next_command_for_coroutine (__main__) [4]
Doctest: __main__.test_pdb_next_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2340, in __main__.test_pdb_next_command_for_coroutine
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_next_command_for_coroutine[4]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_for_coroutine[3]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 195, in run_yielding_async_fn
    try:
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_next_command_for_generator (__main__) [2]
Doctest: __main__.test_pdb_next_command_for_generator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2081, in __main__.test_pdb_next_command_for_generator
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_next_command_for_generator[2]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_pdb_next_command_for_generator[1]>", line 4, in test_function
    try:
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_return_command_for_coroutine (__main__) [0]
Doctest: __main__.test_pdb_return_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2504, in __main__.test_pdb_return_command_for_coroutine
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_return_command_for_coroutine[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_return_command_for_coroutine (__main__) [4]
Doctest: __main__.test_pdb_return_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2519, in __main__.test_pdb_return_command_for_coroutine
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_return_command_for_coroutine[4]>", line 6, in <module>
    test_function()
  File "<doctest __main__.test_pdb_return_command_for_coroutine[3]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 195, in run_yielding_async_fn
    try:
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_return_command_for_generator (__main__) [2]
Doctest: __main__.test_pdb_return_command_for_generator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2462, in __main__.test_pdb_return_command_for_generator
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_return_command_for_generator[2]>", line 9, in <module>
    test_function()
  File "<doctest __main__.test_pdb_return_command_for_generator[1]>", line 4, in test_function
    try:
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_pdb_until_command_for_coroutine (__main__) [0]
Doctest: __main__.test_pdb_until_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2597, in __main__.test_pdb_until_command_for_coroutine
    >>> from test.support import run_yielding_async_fn, async_yield
  File "<doctest __main__.test_pdb_until_command_for_coroutine[0]>", line 1, in <module>
    from test.support import run_yielding_async_fn, async_yield
ImportError: cannot import name 'async_yield' from 'test.support' (/tmp/test/support/__init__.py)

======================================================================
ERROR: test_pdb_until_command_for_coroutine (__main__) [4]
Doctest: __main__.test_pdb_until_command_for_coroutine
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2616, in __main__.test_pdb_until_command_for_coroutine
    >>> with PdbTestInput(['step',
  File "<doctest __main__.test_pdb_until_command_for_coroutine[4]>", line 5, in <module>
    test_function()
  File "<doctest __main__.test_pdb_until_command_for_coroutine[3]>", line 2, in test_function
    run_yielding_async_fn(test_main)
  File "/tmp/test/support/__init__.py", line 196, in run_yielding_async_fn
    coro.send(None)
  File "<doctest __main__.test_pdb_until_command_for_coroutine[2]>", line 3, in test_main
    await test_coro()
  File "<doctest __main__.test_pdb_until_command_for_coroutine[1]>", line 3, in test_coro
    await async_yield(0)
NameError: name 'async_yield' is not defined

======================================================================
ERROR: test_post_mortem (__main__) [2]
Doctest: __main__.test_post_mortem
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1692, in __main__.test_post_mortem
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem[2]>", line 10, in <module>
    try:
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_cause_no_context (__main__) [3]
Doctest: __main__.test_post_mortem_cause_no_context
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_cause_no_context[0]>", line 3, in make_exc_with_stack
    raise type_(*content) from from_
TypeError: The Cause

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_cause_no_context[2]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_cause_no_context[1]>", line 5, in main
    raise ValueError("With Cause") from make_exc_with_stack(TypeError,'The Cause')
ValueError: With Cause

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1376, in __main__.test_post_mortem_cause_no_context
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_cause_no_context[3]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_cause_no_context[2]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_chained (__main__) [3]
Doctest: __main__.test_post_mortem_chained
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_chained[1]>", line 3, in test_function_reraise
    test_function_2()
  File "<doctest __main__.test_post_mortem_chained[0]>", line 3, in test_function_2
    1/0
ZeroDivisionError: division by zero

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_chained[2]>", line 5, in test_function
    test_function_reraise()
  File "<doctest __main__.test_post_mortem_chained[1]>", line 5, in test_function_reraise
    raise ZeroDivisionError('reraised') from e
ZeroDivisionError: reraised

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1294, in __main__.test_post_mortem_chained
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_chained[3]>", line 17, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_chained[2]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_complex (__main__) [7]
Doctest: __main__.test_post_mortem_complex
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[0]>", line 3, in make_ex_with_stack
    raise type_(*content) from from_
ValueError: Cycle2

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[0]>", line 3, in make_ex_with_stack
    raise type_(*content) from from_
ValueError: Cycle1

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[5]>", line 6, in main
    tri_cycle()
  File "<doctest __main__.test_post_mortem_complex[2]>", line 9, in tri_cycle
    raise c from a
  File "<doctest __main__.test_post_mortem_complex[0]>", line 3, in make_ex_with_stack
    raise type_(*content) from from_
ValueError: Cycle3

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_complex[6]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_complex[5]>", line 9, in main
    raise ValueError("With Context and With Cause") from ex
ValueError: With Context and With Cause

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1643, in __main__.test_post_mortem_complex
    >>> with PdbTestInput(  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_complex[7]>", line 10, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_complex[6]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_context_of_the_cause (__main__) [2]
Doctest: __main__.test_post_mortem_context_of_the_cause
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_context_of_the_cause[0]>", line 3, in main
    raise TypeError('Context of the cause')
TypeError: Context of the cause

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_context_of_the_cause[0]>", line 6, in main
    raise ValueError('Root Cause')
ValueError: Root Cause

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_context_of_the_cause[1]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_context_of_the_cause[0]>", line 9, in main
    raise ValueError("With Cause, and cause has context") from ex
ValueError: With Cause, and cause has context

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1430, in __main__.test_post_mortem_context_of_the_cause
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_context_of_the_cause[2]>", line 15, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_context_of_the_cause[1]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_from_no_stack (__main__) [2]
Doctest: __main__.test_post_mortem_from_no_stack
----------------------------------------------------------------------
Exception

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_from_no_stack[1]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_from_no_stack[0]>", line 2, in main
    raise Exception() from Exception()
Exception

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1536, in __main__.test_post_mortem_from_no_stack
    >>> with PdbTestInput(  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_from_no_stack[2]>", line 7, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_from_no_stack[1]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
ERROR: test_post_mortem_from_none (__main__) [2]
Doctest: __main__.test_post_mortem_from_none
----------------------------------------------------------------------
Traceback (most recent call last):
  File "<doctest __main__.test_post_mortem_from_none[1]>", line 5, in test_function
    main()
  File "<doctest __main__.test_post_mortem_from_none[0]>", line 5, in main
    raise ValueError("With Cause, and cause has context") from None
ValueError: With Cause, and cause has context

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1503, in __main__.test_post_mortem_from_none
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
  File "<doctest __main__.test_post_mortem_from_none[2]>", line 6, in <module>
    test_function()
  File "<doctest __main__.test_post_mortem_from_none[1]>", line 7, in test_function
    pdb._post_mortem(e, instance)
  File "/pkg/store/python-0/lib/pdb.py", line 3734, in _post_mortem
    pdb_instance.interaction(None, t)
AttributeError: module 'dis' has no attribute 'findlinestarts'

======================================================================
FAIL: test_find_function_first_executable_line (__main__.PdbTestCase.test_find_function_first_executable_line)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3771, in test_find_function_first_executable_line
    self._assert_find_function(code, 'bar', ('bar', 4))
  File "/tmp/test_pdb.py", line 3605, in _assert_find_function
    self.assertEqual(
AssertionError: Tuples differ: ('bar', '@test_N_tmpæ', 4) != ('bar', '@test_N_tmpæ', 3)

First differing element 2:
4
3

- ('bar', '@test_N_tmpæ', 4)
?                          ^

+ ('bar', '@test_N_tmpæ', 3)
?                          ^


======================================================================
FAIL: test_find_function_found (__main__.PdbTestCase.test_find_function_found)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3641, in test_find_function_found
    self._assert_find_function(
  File "/tmp/test_pdb.py", line 3605, in _assert_find_function
    self.assertEqual(
AssertionError: Tuples differ: ('bœr', '@test_N_tmpæ', 5) != ('bœr', '@test_N_tmpæ', 4)

First differing element 2:
5
4

- ('bœr', '@test_N_tmpæ', 5)
?                          ^

+ ('bœr', '@test_N_tmpæ', 4)
?                          ^


======================================================================
FAIL: test_find_function_found_with_bom (__main__.PdbTestCase.test_find_function_found_with_bom)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3688, in test_find_function_found_with_bom
    self._assert_find_function(
  File "/tmp/test_pdb.py", line 3605, in _assert_find_function
    self.assertEqual(
AssertionError: Tuples differ: ('bœr', '@test_N_tmpæ', 2) != ('bœr', '@test_N_tmpæ', 1)

First differing element 2:
2
1

- ('bœr', '@test_N_tmpæ', 2)
?                          ^

+ ('bœr', '@test_N_tmpæ', 1)
?                          ^


======================================================================
FAIL: test_find_function_found_with_encoding_cookie (__main__.PdbTestCase.test_find_function_found_with_encoding_cookie)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3671, in test_find_function_found_with_encoding_cookie
    self._assert_find_function(
  File "/tmp/test_pdb.py", line 3605, in _assert_find_function
    self.assertEqual(
AssertionError: Tuples differ: ('bœr', '@test_N_tmpæ', 6) != ('bœr', '@test_N_tmpæ', 5)

First differing element 2:
6
5

- ('bœr', '@test_N_tmpæ', 6)
?                          ^

+ ('bœr', '@test_N_tmpæ', 5)
?                          ^


======================================================================
FAIL: test_pyrepl_available (__main__.PdbTestCase.test_pyrepl_available)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 4785, in test_pyrepl_available
    self.assertTrue(pdb._pyrepl_available())
AssertionError: False is not true

======================================================================
FAIL: test_list_commands (__main__) [2]
Doctest: __main__.test_list_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 747, in __main__.test_list_commands
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
        'step',      # go to the test function line
        'list',      # list first function
        'step',      # step into second function
        'list',      # list second function
        'list',      # continue listing to EOF
        'list 1,3',  # list specific lines
        'list x',    # invalid argument
        'next',      # step to import
        'next',      # step over import
        'step',      # step into do_nothing
        'longlist',  # list all lines
        'source do_something',  # list all lines of function
        'source fooxxx',        # something that doesn't exit
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_list_commands[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_list_commands[1]>(3)test_function()
    -> ret = test_function_2('baz')
    (Pdb) list
      1         def test_function():
      2             import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      3  ->         ret = test_function_2('baz')
    [EOF]
    (Pdb) step
    --Call--
    > <doctest test.test_pdb.test_list_commands[0]>(1)test_function_2()
    -> def test_function_2(foo):
    (Pdb) list
      1  ->     def test_function_2(foo):
      2             import test.test_pdb
      3             test.test_pdb.do_nothing()
      4             'some...'
      5             'more...'
      6             'code...'
      7             'to...'
      8             'make...'
      9             'a...'
     10             'long...'
     11             'listing...'
    (Pdb) list
     12             'useful...'
     13             '...'
     14             '...'
     15             return foo
    [EOF]
    (Pdb) list 1,3
      1  ->     def test_function_2(foo):
      2             import test.test_pdb
      3             test.test_pdb.do_nothing()
    (Pdb) list x
    *** ...
    (Pdb) next
    > <doctest test.test_pdb.test_list_commands[0]>(2)test_function_2()
    -> import test.test_pdb
    (Pdb) next
    > <doctest test.test_pdb.test_list_commands[0]>(3)test_function_2()
    -> test.test_pdb.do_nothing()
    (Pdb) step
    --Call--
    > ...test_pdb.py(...)do_nothing()
    -> def do_nothing():
    (Pdb) longlist
    ...  ->     def do_nothing():
    ...             pass
    (Pdb) source do_something
    ...         def do_something():
    ...             print(42)
    (Pdb) source fooxxx
    *** ...
    (Pdb) continue
Got:
    > <doctest __main__.test_list_commands[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_list_commands[1]>(3)test_function()
    -> ret = test_function_2('baz')
    (Pdb) list
      1  	def test_function():
      2  	    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      3  ->	    ret = test_function_2('baz')
    [EOF]
    (Pdb) step
    --Call--
    > <doctest __main__.test_list_commands[0]>(1)test_function_2()
    -> def test_function_2(foo):
    (Pdb) list
      1  ->	def test_function_2(foo):
      2  	    import test.test_pdb
      3  	    test.test_pdb.do_nothing()
      4  	    'some...'
      5  	    'more...'
      6  	    'code...'
      7  	    'to...'
      8  	    'make...'
      9  	    'a...'
     10  	    'long...'
     11  	    'listing...'
    (Pdb) list
     12  	    'useful...'
     13  	    '...'
     14  	    '...'
     15  	    return foo
    [EOF]
    (Pdb) list 1,3
      1  ->	def test_function_2(foo):
      2  	    import test.test_pdb
      3  	    test.test_pdb.do_nothing()
    (Pdb) list x
    *** Error in argument: 'x'
    (Pdb) next
    > <doctest __main__.test_list_commands[0]>(2)test_function_2()
    -> import test.test_pdb
    (Pdb) next
    > <doctest __main__.test_list_commands[0]>(3)test_function_2()
    -> test.test_pdb.do_nothing()
    (Pdb) step
    --Call--
    > /tmp/test/__init__.py(14)__getattr__()
    -> def __getattr__(name):
    (Pdb) longlist
     14  ->	def __getattr__(name):
     15  	    main = sys.modules.get("__main__")
     16  	    path = getattr(main, "__file__", "") if main is not None else ""
     17  	    if path and os.path.basename(path) == name + ".py":
     18  	        sys.modules[__name__ + "." + name] = main
     19  	        return main
     20  	    raise AttributeError(f"cannot import name {name!r} from 'test'")
    (Pdb) source do_something
    *** NameError: name 'do_something' is not defined
    (Pdb) source fooxxx
    *** NameError: name 'fooxxx' is not defined
    (Pdb) continue

======================================================================
FAIL: test_next_until_return_at_return_event (__main__) [2]
Doctest: __main__.test_next_until_return_at_return_event
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2012, in __main__.test_next_until_return_at_return_event
    >>> with PdbTestInput(['break test_function_2',
AssertionError: Failed example:
    with PdbTestInput(['break test_function_2',
                       'continue',
                       'return',
                       'next',
                       'continue',
                       'return',
                       'until',
                       'continue',
                       'return',
                       'return',
                       'continue']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break test_function_2
    Breakpoint 1 at <doctest test.test_pdb.test_next_until_return_at_return_event[0]>:2
    (Pdb) continue
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) next
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(4)test_function()
    -> test_function_2()
    (Pdb) continue
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) until
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(5)test_function()
    -> test_function_2()
    (Pdb) continue
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) return
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(6)test_function()
    -> end = 1
    (Pdb) continue
Got:
    > <doctest __main__.test_next_until_return_at_return_event[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break test_function_2
    Breakpoint 1 at <doctest __main__.test_next_until_return_at_return_event[0]>:1
    (Pdb) continue
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) next
    > <doctest __main__.test_next_until_return_at_return_event[0]>(3)test_function_2()
    -> x = 2
    (Pdb) continue
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) until
    > <doctest __main__.test_next_until_return_at_return_event[0]>(3)test_function_2()
    -> x = 2
    (Pdb) continue
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest __main__.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) continue

======================================================================
FAIL: test_pdb_alias_command (__main__) [2]
Doctest: __main__.test_pdb_alias_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 942, in __main__.test_pdb_alias_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        's',
        'alias pi',
        'alias pi for k in %1.__dict__.keys(): print(f"%1.{k} = {%1.__dict__[k]}")',
        'alias ps pi self',
        'alias ps',
        'pi o',
        's',
        'ps',
        'alias myp p %2',
        'alias myp',
        'alias myp p %1',
        'myp',
        'myp 1',
        'myp 1 2',
        'alias repeat_second_arg p "%* %2"',
        'repeat_second_arg 1 2 3',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_alias_command[1]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest test.test_pdb.test_pdb_alias_command[1]>(4)test_function()
    -> o.method()
    (Pdb) alias pi
    *** Unknown alias 'pi'
    (Pdb) alias pi for k in %1.__dict__.keys(): print(f"%1.{k} = {%1.__dict__[k]}")
    (Pdb) alias ps pi self
    (Pdb) alias ps
    ps = pi self
    (Pdb) pi o
    o.attr1 = 10
    o.attr2 = str
    (Pdb) s
    --Call--
    > <doctest test.test_pdb.test_pdb_alias_command[0]>(5)method()
    -> def method(self):
    (Pdb) ps
    self.attr1 = 10
    self.attr2 = str
    (Pdb) alias myp p %2
    *** Replaceable parameters must be consecutive
    (Pdb) alias myp
    *** Unknown alias 'myp'
    (Pdb) alias myp p %1
    (Pdb) myp
    *** Not enough arguments for alias 'myp'
    (Pdb) myp 1
    1
    (Pdb) myp 1 2
    *** Too many arguments for alias 'myp'
    (Pdb) alias repeat_second_arg p "%* %2"
    (Pdb) repeat_second_arg 1 2 3
    '1 2 3 2'
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_alias_command[1]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest __main__.test_pdb_alias_command[1]>(4)test_function()
    -> o.method()
    (Pdb) alias pi
    *** Unknown alias 'pi'
    (Pdb) alias pi for k in %1.__dict__.keys(): print(f"%1.{k} = {%1.__dict__[k]}")
    (Pdb) alias ps pi self
    (Pdb) alias ps
    ps = pi self
    (Pdb) pi o
    o.attr1 = 10
    *** TypeError: 'NoneType' object is not an iterator
    (Pdb) s
    --Call--
    > <doctest __main__.test_pdb_alias_command[0]>(5)method()
    -> def method(self):
    (Pdb) ps
    self.attr1 = 10
    *** TypeError: 'NoneType' object is not an iterator
    (Pdb) alias myp p %2
    *** Replaceable parameters must be consecutive
    (Pdb) alias myp
    *** Unknown alias 'myp'
    (Pdb) alias myp p %1
    (Pdb) myp
    *** Not enough arguments for alias 'myp'
    (Pdb) myp 1
    1
    (Pdb) myp 1 2
    *** Too many arguments for alias 'myp'
    (Pdb) alias repeat_second_arg p "%* %2"
    (Pdb) repeat_second_arg 1 2 3
    '1 2 3 2'
    (Pdb) continue

======================================================================
FAIL: test_pdb_ambiguous_statements (__main__) [0]
Doctest: __main__.test_pdb_ambiguous_statements
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3295, in __main__.test_pdb_ambiguous_statements
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        's',         # step to the print line
        '! n = 42',  # disambiguated statement: reassign the name n
        'n',         # advance the debugger into the print()
        'continue'
    ]):
        n = -1
        import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
        print(f"The value of n is {n}")
Expected:
    > <doctest test.test_pdb.test_pdb_ambiguous_statements[0]>(8)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest test.test_pdb.test_pdb_ambiguous_statements[0]>(9)<module>()
    -> print(f"The value of n is {n}")
    (Pdb) ! n = 42
    (Pdb) n
    The value of n is 42
    > <doctest test.test_pdb.test_pdb_ambiguous_statements[0]>(1)<module>()
    -> with PdbTestInput([
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_ambiguous_statements[0]>(8)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest __main__.test_pdb_ambiguous_statements[0]>(9)<module>()
    -> print(f"The value of n is {n}")
    (Pdb) ! n = 42
    (Pdb) n
    The value of n is 42
    > <doctest __main__.test_pdb_ambiguous_statements[0]>(1)<module>()
    -> with PdbTestInput([
    (Pdb) continue

======================================================================
FAIL: test_pdb_break_anywhere (__main__) [3]
Doctest: __main__.test_pdb_break_anywhere
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 645, in __main__.test_pdb_break_anywhere
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'b 3',
        'c',
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_break_anywhere[0]>(6)inner()
    -> p.set_trace()
    (Pdb) b 3
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_break_anywhere[0]>:3
    (Pdb) c
    True
    False
    False
Got:
    > <doctest __main__.test_pdb_break_anywhere[0]>(6)inner()
    -> p.set_trace()
    (Pdb) b 3
    Breakpoint 1 at <doctest __main__.test_pdb_break_anywhere[0]>:3
    (Pdb) c
    True
    False
    False

======================================================================
FAIL: test_pdb_breakpoint_on_annotated_function_def (__main__) [4]
Doctest: __main__.test_pdb_breakpoint_on_annotated_function_def
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 420, in __main__.test_pdb_breakpoint_on_annotated_function_def
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'break foo',
        'break bar',
        'break foobar',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[0]>:2
    (Pdb) break bar
    Breakpoint 2 at <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[1]>:2
    (Pdb) break foobar
    Breakpoint 3 at <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[2]>:2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint 1 at <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[0]>:1
    (Pdb) break bar
    Breakpoint 2 at <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[1]>:1
    (Pdb) break foobar
    Breakpoint 3 at <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[2]>:1
    (Pdb) continue

======================================================================
FAIL: test_pdb_breakpoint_with_throw (__main__) [2]
Doctest: __main__.test_pdb_breakpoint_with_throw
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2747, in __main__.test_pdb_breakpoint_with_throw
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'b 7',
        'continue',
        'clear 1',
        'continue',
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b 7
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue
    > <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>(7)test_function()
    -> pass
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_breakpoint_with_throw[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b 7
    Breakpoint 1 at <doctest __main__.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue
    > <doctest __main__.test_pdb_breakpoint_with_throw[1]>(7)test_function()
    -> pass
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest __main__.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue

======================================================================
FAIL: test_pdb_closure (__main__) [3]
Doctest: __main__.test_pdb_closure
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2810, in __main__.test_pdb_closure
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'k',
        'g',
        'y = y',
        'global g; g',
        'global g; (lambda: g)()',
        '(lambda: x)()',
        '(lambda: g)()',
        'lst = [n for n in range(10) if (n % x) == 0]',
        'lst',
        'sum(n for n in lst if n > x)',
        'x = 1; raise Exception()',
        'x',
        'def f():',
        '  return x',
        '',
        'f()',
        'c'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_closure[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) k
    0
    (Pdb) g
    3
    (Pdb) y = y
    *** NameError: name 'y' is not defined
    (Pdb) global g; g
    1
    (Pdb) global g; (lambda: g)()
    1
    (Pdb) (lambda: x)()
    2
    (Pdb) (lambda: g)()
    3
    (Pdb) lst = [n for n in range(10) if (n % x) == 0]
    (Pdb) lst
    [0, 2, 4, 6, 8]
    (Pdb) sum(n for n in lst if n > x)
    18
    (Pdb) x = 1; raise Exception()
    *** Exception
    (Pdb) x
    1
    (Pdb) def f():
    ...     return x
    ...
    (Pdb) f()
    1
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_closure[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) k
    0
    (Pdb) g
    3
    (Pdb) y = y
    *** NameError: name 'y' is not defined
    (Pdb) global g; g
    3
    (Pdb) global g; (lambda: g)()
    1
    (Pdb) (lambda: x)()
    *** TypeError: cannot create 'cell' instances
    (Pdb) (lambda: g)()
    *** TypeError: cannot create 'cell' instances
    (Pdb) lst = [n for n in range(10) if (n % x) == 0]
    *** TypeError: cannot create 'cell' instances
    (Pdb) lst
    *** NameError: name 'lst' is not defined
    (Pdb) sum(n for n in lst if n > x)
    *** TypeError: cannot create 'cell' instances
    (Pdb) x = 1; raise Exception()
    *** Exception
    (Pdb) x
    1
    (Pdb) def f():
    ...     return x
    ...   
    *** TypeError: cannot create 'cell' instances
    (Pdb) f()
    *** NameError: name 'f' is not defined
    (Pdb) c

======================================================================
FAIL: test_pdb_commands_last_breakpoint (__main__) [1]
Doctest: __main__.test_pdb_commands_last_breakpoint
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3490, in __main__.test_pdb_commands_last_breakpoint
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'break 4',
        'break 3',
        'clear 2',
        'commands',
        'p "success"',
        'end',
        'continue',
        'clear 1',
        'commands',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 4
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) break 3
    Breakpoint 2 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) clear 2
    Deleted breakpoint 2 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) commands
    (com) p "success"
    (com) end
    (Pdb) continue
    'success'
    > <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>(4)test_function()
    -> bar = 2
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) commands
    *** cannot set commands: no existing breakpoint
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_commands_last_breakpoint[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 4
    Breakpoint 1 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) break 3
    Breakpoint 2 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) clear 2
    Deleted breakpoint 2 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) commands
    (com) p "success"
    (com) end
    (Pdb) continue
    'success'
    > <doctest __main__.test_pdb_commands_last_breakpoint[0]>(4)test_function()
    -> bar = 2
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) commands
    *** cannot set commands: no existing breakpoint
    (Pdb) continue

======================================================================
FAIL: test_pdb_continue_in_bottomframe (__main__) [1]
Doctest: __main__.test_pdb_continue_in_bottomframe
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1921, in __main__.test_pdb_continue_in_bottomframe
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'step',
        'next',
        'break 7',
        'continue',
        'next',
        'continue',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(3)test_function()
    -> inst.set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(4)test_function()
    -> inst.botframe = sys._getframe()  # hackery to get the right botframe
    (Pdb) next
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(5)test_function()
    -> print(1)
    (Pdb) break 7
    Breakpoint ... at <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>:7
    (Pdb) continue
    1
    2
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(7)test_function()
    -> print(3)
    (Pdb) next
    3
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(8)test_function()
    -> print(4)
    (Pdb) continue
    4
Got:
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(3)test_function()
    -> inst.set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(4)test_function()
    -> inst.botframe = sys._getframe()  # hackery to get the right botframe
    (Pdb) next
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(5)test_function()
    -> print(1)
    (Pdb) break 7
    Breakpoint 1 at <doctest __main__.test_pdb_continue_in_bottomframe[0]>:7
    (Pdb) continue
    1
    2
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(7)test_function()
    -> print(3)
    (Pdb) next
    3
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(8)test_function()
    -> print(4)
    (Pdb) continue
    4

======================================================================
FAIL: test_pdb_displayhook (__main__) [1]
Doctest: __main__.test_pdb_displayhook
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 56, in __main__.test_pdb_displayhook
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'foo',
        'bar',
        'for i in range(5): print(i)',
        'continue',
    ]):
        test_function(1, None)
Expected:
    > <doctest test.test_pdb.test_pdb_displayhook[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) foo
    1
    (Pdb) bar
    (Pdb) for i in range(5): print(i)
    0
    1
    2
    3
    4
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_displayhook[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) foo
    1
    (Pdb) bar
    (Pdb) for i in range(5): print(i)
    0
    *** TypeError: 'NoneType' object is not an iterator
    (Pdb) continue

======================================================================
FAIL: test_pdb_empty_line (__main__) [1]
Doctest: __main__.test_pdb_empty_line
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 694, in __main__.test_pdb_empty_line
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'p x',
        '',  # Should repeat p x
        'n ;; p 0 ;; p x',  # Fill cmdqueue with multiple commands
        '',  # Should still repeat p x
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_empty_line[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p x
    1
    (Pdb)
    1
    (Pdb) n ;; p 0 ;; p x
    0
    1
    > <doctest test.test_pdb.test_pdb_empty_line[0]>(4)test_function()
    -> y = 2
    (Pdb)
    1
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_empty_line[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p x
    1
    (Pdb) 
    1
    (Pdb) n ;; p 0 ;; p x
    0
    1
    > <doctest __main__.test_pdb_empty_line[0]>(4)test_function()
    -> y = 2
    (Pdb) 
    1
    (Pdb) continue

======================================================================
FAIL: test_pdb_f_trace_lines (__main__) [1]
Doctest: __main__.test_pdb_f_trace_lines
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3330, in __main__.test_pdb_f_trace_lines
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'continue'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_f_trace_lines[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_f_trace_lines[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_function_break (__main__) [5]
Doctest: __main__.test_pdb_function_break
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3410, in __main__.test_pdb_function_break
    >>> with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
        'break foo',
        'break bar',
        'break boo',
        'break gen',
        'continue'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_function_break[4]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[0]>:1
    (Pdb) break bar
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[1]>:3
    (Pdb) break boo
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[2]>:4
    (Pdb) break gen
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[3]>:2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_function_break[4]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint 1 at <doctest __main__.test_pdb_function_break[0]>:1
    (Pdb) break bar
    Breakpoint 2 at <doctest __main__.test_pdb_function_break[1]>:1
    (Pdb) break boo
    Breakpoint 3 at <doctest __main__.test_pdb_function_break[2]>:1
    (Pdb) break gen
    Breakpoint 4 at <doctest __main__.test_pdb_function_break[3]>:1
    (Pdb) continue

======================================================================
FAIL: test_pdb_interact_command (__main__) [3]
Doctest: __main__.test_pdb_interact_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1138, in __main__.test_pdb_interact_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
        'interact',
        'x',
        'g',
        'x = 2',
        'g = 3',
        'dict_g["a"] = True',
        'lst_local.append(x)',
        'exit()',
        'p x',
        'p g',
        'p dict_g',
        'p lst_local',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_interact_command[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) interact
    *pdb interact start*
    ... x
    1
    ... g
    0
    ... x = 2
    ... g = 3
    ... dict_g["a"] = True
    ... lst_local.append(x)
    ... exit()
    *exit from pdb interact command*
    (Pdb) p x
    1
    (Pdb) p g
    0
    (Pdb) p dict_g
    {'a': True}
    (Pdb) p lst_local
    [2]
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_interact_command[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) interact
    *pdb interact start*
    >>> x
    1
    >>> g
    0
    >>> x = 2
    >>> g = 3
    >>> dict_g["a"] = True
    >>> lst_local.append(x)
    >>> exit()
    <BLANKLINE>
    *exit from pdb interact command*
    (Pdb) p x
    1
    (Pdb) p g
    0
    (Pdb) p dict_g
    {'a': True}
    (Pdb) p lst_local
    [2]
    (Pdb) continue

======================================================================
FAIL: test_pdb_invalid_arg (__main__) [1]
Doctest: __main__.test_pdb_invalid_arg
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1827, in __main__.test_pdb_invalid_arg
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'a = 3',
        'll 4',
        'step 1',
        'p',
        'enable ',
        'continue'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_invalid_arg[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) a = 3
    *** Invalid argument: = 3
          Usage: a(rgs)
    (Pdb) ll 4
    *** Invalid argument: 4
          Usage: ll | longlist
    (Pdb) step 1
    *** Invalid argument: 1
          Usage: s(tep)
    (Pdb) p
    *** Argument is required for this command
          Usage: p expression
    (Pdb) enable
    *** Argument is required for this command
          Usage: enable bpnumber [bpnumber ...]
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_invalid_arg[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) a = 3
    *** Invalid argument: = 3
          Usage: a(rgs)
    (Pdb) ll 4
    *** Invalid argument: 4
          Usage: ll | longlist
    (Pdb) step 1
    *** Invalid argument: 1
          Usage: s(tep)
    (Pdb) p
    *** Argument is required for this command
          Usage: p expression
    (Pdb) enable 
    *** Argument is required for this command
          Usage: enable bpnumber [bpnumber ...]
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_20766 (__main__) [1]
Doctest: __main__.test_pdb_issue_20766
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2957, in __main__.test_pdb_issue_20766
    >>> with PdbTestInput(['continue',
AssertionError: Failed example:
    with PdbTestInput(['continue',
                       'continue']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 1: <built-in function default_int_handler>
    > <doctest test.test_pdb.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 2: <built-in function default_int_handler>
Got:
    > <doctest __main__.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 1: <built-in function default_int_handler>
    > <doctest __main__.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 2: <built-in function default_int_handler>

======================================================================
FAIL: test_pdb_issue_gh_101517 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_101517
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3193, in __main__.test_pdb_issue_gh_101517
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'continue'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_101517[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_101517[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_101673 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_101673
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3124, in __main__.test_pdb_issue_gh_101673
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        '!a = 2',
        'll',
        'p a',
        'u',
        'p a',
        'd',
        'p a',
        'continue'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) !a = 2
    (Pdb) ll
      1         def test_function():
      2            a = 1
      3  ->        import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) u
    > <doctest test.test_pdb.test_pdb_issue_gh_101673[1]>(11)<module>()
    -> test_function()
    (Pdb) p a
    *** NameError: name 'a' is not defined
    (Pdb) d
    > <doctest test.test_pdb.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) !a = 2
    (Pdb) ll
      1  	def test_function():
      2  	   a = 1
      3  ->	   import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) u
    > <doctest __main__.test_pdb_issue_gh_101673[1]>(11)<module>()
    -> test_function()
    (Pdb) p a
    *** NameError: name 'a' is not defined
    (Pdb) d
    > <doctest __main__.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_103225 (__main__) [0]
Doctest: __main__.test_pdb_issue_gh_103225
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3162, in __main__.test_pdb_issue_gh_103225
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'longlist',
        'continue'
    ]):
        a = 1
        import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
        b = 2
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_103225[0]>(6)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) longlist
      1     with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
      2         'longlist',
      3         'continue'
      4     ]):
      5         a = 1
      6 ->      import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      7         b = 2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_103225[0]>(6)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) longlist
      1  	with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
      2  	    'longlist',
      3  	    'continue'
      4  	]):
      5  	    a = 1
      6  ->	    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      7  	    b = 2
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_108976 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_108976
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3210, in __main__.test_pdb_issue_gh_108976
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'continue'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_108976[0]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_108976[0]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_127321 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_127321
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3226, in __main__.test_pdb_issue_gh_127321
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'continue'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_127321[0]>(4)test_function()
    -> a = 1
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_127321[0]>(3)test_function()
    -> [1, 2] and pdb_instance.set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_65052 (__main__) [2]
Doctest: __main__.test_pdb_issue_gh_65052
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3447, in __main__.test_pdb_issue_gh_65052
    >>> with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
        's',
        's',
        'retval',
        'continue',
        'args',
        'display self',
        'display',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(3)__new__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(4)__new__()
    -> return object.__new__(cls)
    (Pdb) s
    --Return--
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(4)__new__()-><A instance at ...>
    -> return object.__new__(cls)
    (Pdb) retval
    *** repr(retval) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(6)__init__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) args
    self = *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display self
    display self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display
    Currently displaying:
    self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(3)__new__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(4)__new__()
    -> return object.__new__(cls)
    (Pdb) s
    --Return--
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(4)__new__()-><A instance at 0xX>
    -> return object.__new__(cls)
    (Pdb) retval
    *** repr(retval) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(6)__init__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) args
    self = *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display self
    display self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display
    Currently displaying:
    self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_80731 (__main__) [0]
Doctest: __main__.test_pdb_issue_gh_80731
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3272, in __main__.test_pdb_issue_gh_80731
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'import sys',
        'sys.exc_info()',
        'continue'
    ]):
        try:
            raise ValueError('Correct')
        except ValueError:
            import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_80731[0]>(9)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) import sys
    (Pdb) sys.exc_info()
    (<class 'ValueError'>, ValueError('Correct'), <traceback object at ...>)
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_80731[0]>(9)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) import sys
    (Pdb) sys.exc_info()
    (<class 'ValueError'>, ValueError('Correct'), <traceback object at 0xX>)
    (Pdb) continue

======================================================================
FAIL: test_pdb_multiline_statement (__main__) [1]
Doctest: __main__.test_pdb_multiline_statement
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2772, in __main__.test_pdb_multiline_statement
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'def f(x):',
        '  return x * 2',
        '',
        'val = 2',
        'if val > 0:',
        '  val = f(val)',
        '',
        '',  # empty line should repeat the multi-line statement
        'val',
        'c'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_multiline_statement[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) def f(x):
    ...     return x * 2
    ...
    (Pdb) val = 2
    (Pdb) if val > 0:
    ...     val = f(val)
    ...
    (Pdb)
    (Pdb) val
    8
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_multiline_statement[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) def f(x):
    ...     return x * 2
    ...   
    *** TypeError: cannot create 'cell' instances
    (Pdb) val = 2
    (Pdb) if val > 0:
    ...     val = f(val)
    ...   
    *** NameError: name 'f' is not defined
    (Pdb) 
    *** NameError: name 'f' is not defined
    (Pdb) val
    2
    (Pdb) c

======================================================================
FAIL: test_pdb_pp_repr_exc (__main__) [3]
Doctest: __main__.test_pdb_pp_repr_exc
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 671, in __main__.test_pdb_pp_repr_exc
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'p obj',
        'pp obj',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_pp_repr_exc[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p obj
    *** Exception: repr_exc
    (Pdb) pp obj
    *** Exception: repr_exc
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_pp_repr_exc[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p obj
    *** Exception: repr_exc
    (Pdb) pp obj
    *** Exception: repr_exc
    (Pdb) continue

======================================================================
FAIL: test_pdb_restart_command (__main__) [1]
Doctest: __main__.test_pdb_restart_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1098, in __main__.test_pdb_restart_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'restart',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_restart_command[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False, mode='inline').set_trace()
    (Pdb) restart
    *** run/restart command is disabled when pdb is running in inline mode.
    Use the command line interface to enable restarting your program
    e.g. "python -m pdb myscript.py"
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_restart_command[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False, mode='inline').set_trace()
    (Pdb) restart
    *** run/restart command is disabled when pdb is running in inline mode.
    Use the command line interface to enable restarting your program
    e.g. "python -m pdb myscript.py"
    (Pdb) continue

======================================================================
FAIL: test_pdb_return_to_different_file (__main__) [3]
Doctest: __main__.test_pdb_return_to_different_file
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1758, in __main__.test_pdb_return_to_different_file
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
        'b A.__repr__',
        'continue',
        'return',
        'next',
        'return',
        'return',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_return_to_different_file[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b A.__repr__
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_return_to_different_file[1]>:3
    (Pdb) continue
    > <doctest test.test_pdb.test_pdb_return_to_different_file[1]>(3)__repr__()
    -> return 'A'
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_pdb_return_to_different_file[1]>(3)__repr__()->'A'
    -> return 'A'
    (Pdb) next
    > ...pprint.py..._safe_repr()
    -> return rep,...
    (Pdb) return
    --Return--
    > ...pprint.py..._safe_repr()->('A'...)
    -> return rep,...
    (Pdb) return
    --Return--
    > ...pprint.py...format()->('A'...)
    -> return...
    (Pdb) continue
    A
Got:
    > <doctest __main__.test_pdb_return_to_different_file[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b A.__repr__
    Breakpoint 1 at <doctest __main__.test_pdb_return_to_different_file[1]>:2
    (Pdb) continue
    > <doctest __main__.test_pdb_return_to_different_file[1]>(3)__repr__()
    -> return 'A'
    (Pdb) return
    > <doctest __main__.test_pdb_return_to_different_file[1]>(3)__repr__()
    -> return 'A'
    (Pdb) next
    --Return--
    > <doctest __main__.test_pdb_return_to_different_file[1]>(3)__repr__()->'A'
    -> return 'A'
    (Pdb) return
    > /pkg/store/python-0/lib/pprint.py(920)_safe_repr()
    -> return rep, (rep and not rep.startswith('<')), False
    (Pdb) return
    --Return--
    > /pkg/store/python-0/lib/pprint.py(920)_safe_repr()->('A', True, False)
    -> return rep, (rep and not rep.startswith('<')), False
    (Pdb) continue
    A

======================================================================
FAIL: test_pdb_run_with_incorrect_argument (__main__) [1]
Doctest: __main__.test_pdb_run_with_incorrect_argument
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1964, in __main__.test_pdb_run_with_incorrect_argument
    >>> with pti:
AssertionError: Failed example:
    with pti:
        pdb_invoke('run', lambda x: x)
Expected:
    Traceback (most recent call last):
    TypeError: exec() arg 1 must be a string, bytes or code object
Got:
    Traceback (most recent call last):
      File "<doctest __main__.test_pdb_run_with_incorrect_argument[1]>", line 2, in <module>
        pdb_invoke('run', lambda x: x)
      File "/tmp/test_pdb.py", line 1957, in pdb_invoke
        getattr(pdb.Pdb(nosigint=True, readrc=False), method)(arg)
      File "/pkg/store/python-0/lib/bdb.py", line 904, in run
        exec(cmd, globals, locals)
    TypeError: source must be a string, bytes or a code object: function

======================================================================
FAIL: test_pdb_run_with_incorrect_argument (__main__) [2]
Doctest: __main__.test_pdb_run_with_incorrect_argument
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1969, in __main__.test_pdb_run_with_incorrect_argument
    >>> with pti:
AssertionError: Failed example:
    with pti:
        pdb_invoke('runeval', lambda x: x)
Expected:
    Traceback (most recent call last):
    TypeError: eval() arg 1 must be a string, bytes or code object
Got:
    Traceback (most recent call last):
      File "<doctest __main__.test_pdb_run_with_incorrect_argument[2]>", line 2, in <module>
        pdb_invoke('runeval', lambda x: x)
      File "/tmp/test_pdb.py", line 1957, in pdb_invoke
        getattr(pdb.Pdb(nosigint=True, readrc=False), method)(arg)
      File "/pkg/store/python-0/lib/bdb.py", line 924, in runeval
        return eval(expr, globals, locals)
    TypeError: source must be a string, bytes or a code object: function

======================================================================
FAIL: test_pdb_show_attribute_and_item (__main__) [1]
Doctest: __main__.test_pdb_show_attribute_and_item
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2871, in __main__.test_pdb_show_attribute_and_item
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'c["a"]',
        'c.get("a")',
        'n(1)',
        'j=1',
        'j+1',
        'r"a"',
        'next(iter([1]))',
        'list((0, 1))',
        'c'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_show_attribute_and_item[0]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) c["a"]
    1
    (Pdb) c.get("a")
    1
    (Pdb) n(1)
    1
    (Pdb) j=1
    (Pdb) j+1
    2
    (Pdb) r"a"
    'a'
    (Pdb) next(iter([1]))
    1
    (Pdb) list((0, 1))
    [0, 1]
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_show_attribute_and_item[0]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) c["a"]
    1
    (Pdb) c.get("a")
    1
    (Pdb) n(1)
    1
    (Pdb) j=1
    (Pdb) j+1
    2
    (Pdb) r"a"
    'a'
    (Pdb) next(iter([1]))
    1
    (Pdb) list((0, 1))
    [0, 1]
    (Pdb) c

======================================================================
FAIL: test_pdb_skip_modules (__main__) [1]
Doctest: __main__.test_pdb_skip_modules
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1803, in __main__.test_pdb_skip_modules
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'step',
        'step',
        'continue',
    ]):
        skip_module()
Expected:
    > <doctest test.test_pdb.test_pdb_skip_modules[0]>(3)skip_module()
    -> import pdb; pdb.Pdb(skip=['stri*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules[0]>(4)skip_module()
    -> string.capwords('FOO')
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_skip_modules[0]>(4)skip_module()->None
    -> string.capwords('FOO')
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_skip_modules[0]>(3)skip_module()
    -> import pdb; pdb.Pdb(skip=['stri*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules[0]>(4)skip_module()
    -> string.capwords('FOO')
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_skip_modules[0]>(4)skip_module()->None
    -> string.capwords('FOO')
    (Pdb) continue

======================================================================
FAIL: test_pdb_skip_modules_with_callback (__main__) [1]
Doctest: __main__.test_pdb_skip_modules_with_callback
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1871, in __main__.test_pdb_skip_modules_with_callback
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'step',
        'step',
        'step',
        'step',
        'step',
        'step',
        'continue',
    ]):
        skip_module()
        pass  # provides something to "step" to
Expected:
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(4)skip_module()
    -> import pdb; pdb.Pdb(skip=['module_to_skip*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(5)skip_module()
    -> mod.foo_pony(callback)
    (Pdb) step
    --Call--
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(2)callback()
    -> def callback():
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(3)callback()
    -> return None
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(3)callback()->None
    -> return None
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(5)skip_module()->None
    -> mod.foo_pony(callback)
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[1]>(11)<module>()
    -> pass  # provides something to "step" to
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(4)skip_module()
    -> import pdb; pdb.Pdb(skip=['module_to_skip*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(5)skip_module()
    -> mod.foo_pony(callback)
    (Pdb) step
    --Call--
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(2)callback()
    -> def callback():
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(3)callback()
    -> return None
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(3)callback()->None
    -> return None
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(5)skip_module()->None
    -> mod.foo_pony(callback)
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules_with_callback[1]>(11)<module>()
    -> pass  # provides something to "step" to
    (Pdb) continue

======================================================================
FAIL: test_pdb_whatis_command (__main__) [4]
Doctest: __main__.test_pdb_whatis_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 837, in __main__.test_pdb_whatis_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
       'whatis myvar',
       'whatis myfunc',
       'whatis MyClass',
       'whatis MyClass()',
       'whatis MyClass.mymethod',
       'whatis MyClass().mymethod',
       'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_whatis_command[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) whatis myvar
    <class 'tuple'>
    (Pdb) whatis myfunc
    Function myfunc
    (Pdb) whatis MyClass
    Class test.test_pdb.MyClass
    (Pdb) whatis MyClass()
    <class 'test.test_pdb.MyClass'>
    (Pdb) whatis MyClass.mymethod
    Function mymethod
    (Pdb) whatis MyClass().mymethod
    Method mymethod
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_whatis_command[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) whatis myvar
    <class 'tuple'>
    (Pdb) whatis myfunc
    Function myfunc
    (Pdb) whatis MyClass
    Class __main__.MyClass
    (Pdb) whatis MyClass()
    <class '__main__.MyClass'>
    (Pdb) whatis MyClass.mymethod
    Function mymethod
    (Pdb) whatis MyClass().mymethod
    Method mymethod
    (Pdb) continue

======================================================================
FAIL: test_pdb_where_command (__main__) [3]
Doctest: __main__.test_pdb_where_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1012, in __main__.test_pdb_where_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'w',
        'where',
        'w 1',
        'w invalid',
        'u',
        'w',
        'w 0',
        'w 100',
        'w -100',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) where
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 1
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w invalid
    *** Invalid count (invalid)
    (Pdb) u
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 0
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w 100
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w -100
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) where
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 1
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w invalid
    *** Invalid count (invalid)
    (Pdb) u
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 0
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w 100
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w -100
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_with_inline_breakpoint (__main__) [1]
Doctest: __main__.test_pdb_with_inline_breakpoint
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2916, in __main__.test_pdb_with_inline_breakpoint
    >>> with PdbTestInput(['display x',
AssertionError: Failed example:
    with PdbTestInput(['display x',
                       'n',
                       'n',
                       'n',
                       'n',
                       'undisplay',
                       'c']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(3)test_function()
    -> import pdb; pdb.Pdb().set_trace()
    (Pdb) display x
    display x: 1
    (Pdb) n
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(5)test_function()
    -> x = 2
    (Pdb) n
    --Return--
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(5)test_function()->None
    -> x = 2
    display x: 2  [old: 1]
    (Pdb) undisplay
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(3)test_function()
    -> import pdb; pdb.Pdb().set_trace()
    (Pdb) display x
    display x: 1
    (Pdb) n
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(5)test_function()
    -> x = 2
    (Pdb) n
    --Return--
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(5)test_function()->None
    -> x = 2
    display x: 2  [old: 1]
    (Pdb) undisplay
    (Pdb) c

======================================================================
FAIL: test_post_mortem (__main__) [2]
Doctest: __main__.test_post_mortem
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1692, in __main__.test_post_mortem
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
        'step',      # step to test_function_2() line
        'next',      # step over exception-raising call
        'bt',        # get a backtrace
        'list',      # list code of test_function()
        'down',      # step into test_function_2()
        'list',      # list code of test_function_2()
        'continue',
    ]):
       try:
           test_function()
       except ZeroDivisionError:
           print('Correctly reraised.')
Expected:
    > <doctest test.test_pdb.test_post_mortem[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_post_mortem[1]>(3)test_function()
    -> test_function_2()
    (Pdb) next
    Exception!
    ZeroDivisionError: division by zero
    > <doctest test.test_pdb.test_post_mortem[1]>(3)test_function()
    -> test_function_2()
    (Pdb) bt
    ...
      <doctest test.test_pdb.test_post_mortem[2]>(11)<module>()
    -> test_function()
    > <doctest test.test_pdb.test_post_mortem[1]>(3)test_function()
    -> test_function_2()
      <doctest test.test_pdb.test_post_mortem[0]>(3)test_function_2()
    -> 1/0
    (Pdb) list
      1         def test_function():
      2             import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      3  ->         test_function_2()
      4             print('Not reached.')
    [EOF]
    (Pdb) down
    > <doctest test.test_pdb.test_post_mortem[0]>(3)test_function_2()
    -> 1/0
    (Pdb) list
      1         def test_function_2():
      2             try:
      3  >>             1/0
      4             finally:
      5  ->             print('Exception!')
    [EOF]
    (Pdb) continue
    Correctly reraised.
Got:
    > <doctest __main__.test_post_mortem[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_post_mortem[1]>(3)test_function()
    -> test_function_2()
    (Pdb) next
    Exception!
    Correctly reraised.

======================================================================
FAIL: test_list_commands (__main__) [2]
Doctest: __main__.test_list_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 747, in __main__.test_list_commands
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
        'step',      # go to the test function line
        'list',      # list first function
        'step',      # step into second function
        'list',      # list second function
        'list',      # continue listing to EOF
        'list 1,3',  # list specific lines
        'list x',    # invalid argument
        'next',      # step to import
        'next',      # step over import
        'step',      # step into do_nothing
        'longlist',  # list all lines
        'source do_something',  # list all lines of function
        'source fooxxx',        # something that doesn't exit
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_list_commands[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_list_commands[1]>(3)test_function()
    -> ret = test_function_2('baz')
    (Pdb) list
      1         def test_function():
      2             import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      3  ->         ret = test_function_2('baz')
    [EOF]
    (Pdb) step
    --Call--
    > <doctest test.test_pdb.test_list_commands[0]>(1)test_function_2()
    -> def test_function_2(foo):
    (Pdb) list
      1  ->     def test_function_2(foo):
      2             import test.test_pdb
      3             test.test_pdb.do_nothing()
      4             'some...'
      5             'more...'
      6             'code...'
      7             'to...'
      8             'make...'
      9             'a...'
     10             'long...'
     11             'listing...'
    (Pdb) list
     12             'useful...'
     13             '...'
     14             '...'
     15             return foo
    [EOF]
    (Pdb) list 1,3
      1  ->     def test_function_2(foo):
      2             import test.test_pdb
      3             test.test_pdb.do_nothing()
    (Pdb) list x
    *** ...
    (Pdb) next
    > <doctest test.test_pdb.test_list_commands[0]>(2)test_function_2()
    -> import test.test_pdb
    (Pdb) next
    > <doctest test.test_pdb.test_list_commands[0]>(3)test_function_2()
    -> test.test_pdb.do_nothing()
    (Pdb) step
    --Call--
    > ...test_pdb.py(...)do_nothing()
    -> def do_nothing():
    (Pdb) longlist
    ...  ->     def do_nothing():
    ...             pass
    (Pdb) source do_something
    ...         def do_something():
    ...             print(42)
    (Pdb) source fooxxx
    *** ...
    (Pdb) continue
Got:
    > <doctest __main__.test_list_commands[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_list_commands[1]>(3)test_function()
    -> ret = test_function_2('baz')
    (Pdb) list
      1  	def test_function():
      2  	    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      3  ->	    ret = test_function_2('baz')
    [EOF]
    (Pdb) step
    --Call--
    > <doctest __main__.test_list_commands[0]>(1)test_function_2()
    -> def test_function_2(foo):
    (Pdb) list
      1  ->	def test_function_2(foo):
      2  	    import test.test_pdb
      3  	    test.test_pdb.do_nothing()
      4  	    'some...'
      5  	    'more...'
      6  	    'code...'
      7  	    'to...'
      8  	    'make...'
      9  	    'a...'
     10  	    'long...'
     11  	    'listing...'
    (Pdb) list
     12  	    'useful...'
     13  	    '...'
     14  	    '...'
     15  	    return foo
    [EOF]
    (Pdb) list 1,3
      1  ->	def test_function_2(foo):
      2  	    import test.test_pdb
      3  	    test.test_pdb.do_nothing()
    (Pdb) list x
    *** Error in argument: 'x'
    (Pdb) next
    > <doctest __main__.test_list_commands[0]>(2)test_function_2()
    -> import test.test_pdb
    (Pdb) next
    > <doctest __main__.test_list_commands[0]>(3)test_function_2()
    -> test.test_pdb.do_nothing()
    (Pdb) step
    --Call--
    > /tmp/test/__init__.py(14)__getattr__()
    -> def __getattr__(name):
    (Pdb) longlist
     14  ->	def __getattr__(name):
     15  	    main = sys.modules.get("__main__")
     16  	    path = getattr(main, "__file__", "") if main is not None else ""
     17  	    if path and os.path.basename(path) == name + ".py":
     18  	        sys.modules[__name__ + "." + name] = main
     19  	        return main
     20  	    raise AttributeError(f"cannot import name {name!r} from 'test'")
    (Pdb) source do_something
    *** NameError: name 'do_something' is not defined
    (Pdb) source fooxxx
    *** NameError: name 'fooxxx' is not defined
    (Pdb) continue

======================================================================
FAIL: test_next_until_return_at_return_event (__main__) [2]
Doctest: __main__.test_next_until_return_at_return_event
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2012, in __main__.test_next_until_return_at_return_event
    >>> with PdbTestInput(['break test_function_2',
AssertionError: Failed example:
    with PdbTestInput(['break test_function_2',
                       'continue',
                       'return',
                       'next',
                       'continue',
                       'return',
                       'until',
                       'continue',
                       'return',
                       'return',
                       'continue']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break test_function_2
    Breakpoint 1 at <doctest test.test_pdb.test_next_until_return_at_return_event[0]>:2
    (Pdb) continue
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) next
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(4)test_function()
    -> test_function_2()
    (Pdb) continue
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) until
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(5)test_function()
    -> test_function_2()
    (Pdb) continue
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) return
    > <doctest test.test_pdb.test_next_until_return_at_return_event[1]>(6)test_function()
    -> end = 1
    (Pdb) continue
Got:
    > <doctest __main__.test_next_until_return_at_return_event[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break test_function_2
    Breakpoint 1 at <doctest __main__.test_next_until_return_at_return_event[0]>:1
    (Pdb) continue
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest __main__.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) next
    > <doctest __main__.test_next_until_return_at_return_event[1]>(4)test_function()
    -> test_function_2()
    (Pdb) continue
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest __main__.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) until
    > <doctest __main__.test_next_until_return_at_return_event[1]>(5)test_function()
    -> test_function_2()
    (Pdb) continue
    > <doctest __main__.test_next_until_return_at_return_event[0]>(2)test_function_2()
    -> x = 1
    (Pdb) return
    --Return--
    > <doctest __main__.test_next_until_return_at_return_event[0]>(3)test_function_2()->None
    -> x = 2
    (Pdb) return
    > <doctest __main__.test_next_until_return_at_return_event[1]>(6)test_function()
    -> end = 1
    (Pdb) continue

======================================================================
FAIL: test_pdb_alias_command (__main__) [2]
Doctest: __main__.test_pdb_alias_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 942, in __main__.test_pdb_alias_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        's',
        'alias pi',
        'alias pi for k in %1.__dict__.keys(): print(f"%1.{k} = {%1.__dict__[k]}")',
        'alias ps pi self',
        'alias ps',
        'pi o',
        's',
        'ps',
        'alias myp p %2',
        'alias myp',
        'alias myp p %1',
        'myp',
        'myp 1',
        'myp 1 2',
        'alias repeat_second_arg p "%* %2"',
        'repeat_second_arg 1 2 3',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_alias_command[1]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest test.test_pdb.test_pdb_alias_command[1]>(4)test_function()
    -> o.method()
    (Pdb) alias pi
    *** Unknown alias 'pi'
    (Pdb) alias pi for k in %1.__dict__.keys(): print(f"%1.{k} = {%1.__dict__[k]}")
    (Pdb) alias ps pi self
    (Pdb) alias ps
    ps = pi self
    (Pdb) pi o
    o.attr1 = 10
    o.attr2 = str
    (Pdb) s
    --Call--
    > <doctest test.test_pdb.test_pdb_alias_command[0]>(5)method()
    -> def method(self):
    (Pdb) ps
    self.attr1 = 10
    self.attr2 = str
    (Pdb) alias myp p %2
    *** Replaceable parameters must be consecutive
    (Pdb) alias myp
    *** Unknown alias 'myp'
    (Pdb) alias myp p %1
    (Pdb) myp
    *** Not enough arguments for alias 'myp'
    (Pdb) myp 1
    1
    (Pdb) myp 1 2
    *** Too many arguments for alias 'myp'
    (Pdb) alias repeat_second_arg p "%* %2"
    (Pdb) repeat_second_arg 1 2 3
    '1 2 3 2'
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_alias_command[1]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest __main__.test_pdb_alias_command[1]>(4)test_function()
    -> o.method()
    (Pdb) alias pi
    *** Unknown alias 'pi'
    (Pdb) alias pi for k in %1.__dict__.keys(): print(f"%1.{k} = {%1.__dict__[k]}")
    (Pdb) alias ps pi self
    (Pdb) alias ps
    ps = pi self
    (Pdb) pi o
    o.attr1 = 10
    *** TypeError: 'NoneType' object is not an iterator
    (Pdb) s
    --Call--
    > <doctest __main__.test_pdb_alias_command[0]>(5)method()
    -> def method(self):
    (Pdb) ps
    self.attr1 = 10
    *** TypeError: 'NoneType' object is not an iterator
    (Pdb) alias myp p %2
    *** Replaceable parameters must be consecutive
    (Pdb) alias myp
    *** Unknown alias 'myp'
    (Pdb) alias myp p %1
    (Pdb) myp
    *** Not enough arguments for alias 'myp'
    (Pdb) myp 1
    1
    (Pdb) myp 1 2
    *** Too many arguments for alias 'myp'
    (Pdb) alias repeat_second_arg p "%* %2"
    (Pdb) repeat_second_arg 1 2 3
    '1 2 3 2'
    (Pdb) continue

======================================================================
FAIL: test_pdb_ambiguous_statements (__main__) [0]
Doctest: __main__.test_pdb_ambiguous_statements
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3295, in __main__.test_pdb_ambiguous_statements
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        's',         # step to the print line
        '! n = 42',  # disambiguated statement: reassign the name n
        'n',         # advance the debugger into the print()
        'continue'
    ]):
        n = -1
        import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
        print(f"The value of n is {n}")
Expected:
    > <doctest test.test_pdb.test_pdb_ambiguous_statements[0]>(8)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest test.test_pdb.test_pdb_ambiguous_statements[0]>(9)<module>()
    -> print(f"The value of n is {n}")
    (Pdb) ! n = 42
    (Pdb) n
    The value of n is 42
    > <doctest test.test_pdb.test_pdb_ambiguous_statements[0]>(1)<module>()
    -> with PdbTestInput([
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_ambiguous_statements[0]>(8)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest __main__.test_pdb_ambiguous_statements[0]>(9)<module>()
    -> print(f"The value of n is {n}")
    (Pdb) ! n = 42
    (Pdb) n
    The value of n is 42
    > <doctest __main__.test_pdb_ambiguous_statements[0]>(1)<module>()
    -> with PdbTestInput([
    (Pdb) continue

======================================================================
FAIL: test_pdb_break_anywhere (__main__) [3]
Doctest: __main__.test_pdb_break_anywhere
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 645, in __main__.test_pdb_break_anywhere
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'b 3',
        'c',
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_break_anywhere[0]>(6)inner()
    -> p.set_trace()
    (Pdb) b 3
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_break_anywhere[0]>:3
    (Pdb) c
    True
    False
    False
Got:
    > <doctest __main__.test_pdb_break_anywhere[0]>(6)inner()
    -> p.set_trace()
    (Pdb) b 3
    Breakpoint 1 at <doctest __main__.test_pdb_break_anywhere[0]>:3
    (Pdb) c
    True
    False
    False

======================================================================
FAIL: test_pdb_breakpoint_ignore_and_condition (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_ignore_and_condition
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 370, in __main__.test_pdb_breakpoint_ignore_and_condition
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'break 4',
        'ignore 1 2',  # ignore once
        'continue',
        'condition 1 i == 4',
        'continue',
        'clear 1',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_breakpoint_ignore_and_condition[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 4
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_ignore_and_condition[0]>:4
    (Pdb) ignore 1 2
    Will ignore next 2 crossings of breakpoint 1.
    (Pdb) continue
    0
    1
    > <doctest test.test_pdb.test_pdb_breakpoint_ignore_and_condition[0]>(4)test_function()
    -> print(i)
    (Pdb) condition 1 i == 4
    New condition set for breakpoint 1.
    (Pdb) continue
    2
    3
    > <doctest test.test_pdb.test_pdb_breakpoint_ignore_and_condition[0]>(4)test_function()
    -> print(i)
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_ignore_and_condition[0]>:4
    (Pdb) continue
    4
Got:
    > <doctest __main__.test_pdb_breakpoint_ignore_and_condition[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 4
    Breakpoint 1 at <doctest __main__.test_pdb_breakpoint_ignore_and_condition[0]>:4
    (Pdb) ignore 1 2
    Will ignore next 2 crossings of breakpoint 1.
    (Pdb) continue
    0
    1
    > <doctest __main__.test_pdb_breakpoint_ignore_and_condition[0]>(4)test_function()
    -> print(i)
    (Pdb) condition 1 i == 4
    New condition set for breakpoint 1.
    (Pdb) continue
    2
    3
    > <doctest __main__.test_pdb_breakpoint_ignore_and_condition[0]>(4)test_function()
    -> print(i)
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest __main__.test_pdb_breakpoint_ignore_and_condition[0]>:4
    (Pdb) continue
    4

======================================================================
FAIL: test_pdb_breakpoint_on_annotated_function_def (__main__) [4]
Doctest: __main__.test_pdb_breakpoint_on_annotated_function_def
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 420, in __main__.test_pdb_breakpoint_on_annotated_function_def
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'break foo',
        'break bar',
        'break foobar',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[0]>:2
    (Pdb) break bar
    Breakpoint 2 at <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[1]>:2
    (Pdb) break foobar
    Breakpoint 3 at <doctest test.test_pdb.test_pdb_breakpoint_on_annotated_function_def[2]>:2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint 1 at <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[0]>:1
    (Pdb) break bar
    Breakpoint 2 at <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[1]>:1
    (Pdb) break foobar
    Breakpoint 3 at <doctest __main__.test_pdb_breakpoint_on_annotated_function_def[2]>:1
    (Pdb) continue

======================================================================
FAIL: test_pdb_breakpoint_on_disabled_line (__main__) [1]
Doctest: __main__.test_pdb_breakpoint_on_disabled_line
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 526, in __main__.test_pdb_breakpoint_on_disabled_line
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'break 5',
        'c',
        'clear 1',
        'break 4',
        'c',
        'clear 2',
        'c'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_breakpoint_on_disabled_line[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 5
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_on_disabled_line[0]>:5
    (Pdb) c
    > <doctest test.test_pdb.test_pdb_breakpoint_on_disabled_line[0]>(5)test_function()
    -> print(j)
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_on_disabled_line[0]>:5
    (Pdb) break 4
    Breakpoint 2 at <doctest test.test_pdb.test_pdb_breakpoint_on_disabled_line[0]>:4
    (Pdb) c
    0
    > <doctest test.test_pdb.test_pdb_breakpoint_on_disabled_line[0]>(4)test_function()
    -> j = i * 2
    (Pdb) clear 2
    Deleted breakpoint 2 at <doctest test.test_pdb.test_pdb_breakpoint_on_disabled_line[0]>:4
    (Pdb) c
    2
    4
Got:
    > <doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 5
    Breakpoint 1 at <doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>:5
    (Pdb) c
    > <doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>(5)test_function()
    -> print(j)
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>:5
    (Pdb) break 4
    Breakpoint 2 at <doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>:4
    (Pdb) c
    0
    > <doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>(4)test_function()
    -> j = i * 2
    (Pdb) clear 2
    Deleted breakpoint 2 at <doctest __main__.test_pdb_breakpoint_on_disabled_line[0]>:4
    (Pdb) c
    2
    4

======================================================================
FAIL: test_pdb_breakpoint_with_throw (__main__) [2]
Doctest: __main__.test_pdb_breakpoint_with_throw
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2747, in __main__.test_pdb_breakpoint_with_throw
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'b 7',
        'continue',
        'clear 1',
        'continue',
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b 7
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue
    > <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>(7)test_function()
    -> pass
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest test.test_pdb.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_breakpoint_with_throw[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b 7
    Breakpoint 1 at <doctest __main__.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue
    > <doctest __main__.test_pdb_breakpoint_with_throw[1]>(7)test_function()
    -> pass
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest __main__.test_pdb_breakpoint_with_throw[1]>:7
    (Pdb) continue

======================================================================
FAIL: test_pdb_closure (__main__) [3]
Doctest: __main__.test_pdb_closure
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2810, in __main__.test_pdb_closure
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'k',
        'g',
        'y = y',
        'global g; g',
        'global g; (lambda: g)()',
        '(lambda: x)()',
        '(lambda: g)()',
        'lst = [n for n in range(10) if (n % x) == 0]',
        'lst',
        'sum(n for n in lst if n > x)',
        'x = 1; raise Exception()',
        'x',
        'def f():',
        '  return x',
        '',
        'f()',
        'c'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_closure[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) k
    0
    (Pdb) g
    3
    (Pdb) y = y
    *** NameError: name 'y' is not defined
    (Pdb) global g; g
    1
    (Pdb) global g; (lambda: g)()
    1
    (Pdb) (lambda: x)()
    2
    (Pdb) (lambda: g)()
    3
    (Pdb) lst = [n for n in range(10) if (n % x) == 0]
    (Pdb) lst
    [0, 2, 4, 6, 8]
    (Pdb) sum(n for n in lst if n > x)
    18
    (Pdb) x = 1; raise Exception()
    *** Exception
    (Pdb) x
    1
    (Pdb) def f():
    ...     return x
    ...
    (Pdb) f()
    1
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_closure[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) k
    0
    (Pdb) g
    3
    (Pdb) y = y
    *** NameError: name 'y' is not defined
    (Pdb) global g; g
    3
    (Pdb) global g; (lambda: g)()
    1
    (Pdb) (lambda: x)()
    *** TypeError: cannot create 'cell' instances
    (Pdb) (lambda: g)()
    *** TypeError: cannot create 'cell' instances
    (Pdb) lst = [n for n in range(10) if (n % x) == 0]
    *** TypeError: cannot create 'cell' instances
    (Pdb) lst
    *** NameError: name 'lst' is not defined
    (Pdb) sum(n for n in lst if n > x)
    *** TypeError: cannot create 'cell' instances
    (Pdb) x = 1; raise Exception()
    *** Exception
    (Pdb) x
    1
    (Pdb) def f():
    ...     return x
    ...   
    *** TypeError: cannot create 'cell' instances
    (Pdb) f()
    *** NameError: name 'f' is not defined
    (Pdb) c

======================================================================
FAIL: test_pdb_commands (__main__) [1]
Doctest: __main__.test_pdb_commands
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 447, in __main__.test_pdb_commands
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'b 3',
        'commands',
        'silent',      # suppress the frame status output
        'p "hello"',
        'end',
        'b 4',
        'commands',
        'until 5',     # no output, should stop at line 5
        'continue',    # hit breakpoint at line 3
        '',            # repeat continue, hit breakpoint at line 4 then `until` to line 5
        '',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_commands[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b 3
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_commands[0]>:3
    (Pdb) commands
    (com) silent
    (com) p "hello"
    (com) end
    (Pdb) b 4
    Breakpoint 2 at <doctest test.test_pdb.test_pdb_commands[0]>:4
    (Pdb) commands
    (com) until 5
    (Pdb) continue
    'hello'
    (Pdb)
    1
    2
    > <doctest test.test_pdb.test_pdb_commands[0]>(5)test_function()
    -> print(3)
    (Pdb)
    3
Got:
    > <doctest __main__.test_pdb_commands[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b 3
    Breakpoint 1 at <doctest __main__.test_pdb_commands[0]>:3
    (Pdb) commands
    (com) silent
    (com) p "hello"
    (com) end
    (Pdb) b 4
    Breakpoint 2 at <doctest __main__.test_pdb_commands[0]>:4
    (Pdb) commands
    (com) until 5
    (Pdb) continue
    'hello'
    (Pdb) 
    1
    2
    > <doctest __main__.test_pdb_commands[0]>(5)test_function()
    -> print(3)
    (Pdb) 
    3

======================================================================
FAIL: test_pdb_commands_last_breakpoint (__main__) [1]
Doctest: __main__.test_pdb_commands_last_breakpoint
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3490, in __main__.test_pdb_commands_last_breakpoint
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'break 4',
        'break 3',
        'clear 2',
        'commands',
        'p "success"',
        'end',
        'continue',
        'clear 1',
        'commands',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 4
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) break 3
    Breakpoint 2 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) clear 2
    Deleted breakpoint 2 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) commands
    (com) p "success"
    (com) end
    (Pdb) continue
    'success'
    > <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>(4)test_function()
    -> bar = 2
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest test.test_pdb.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) commands
    *** cannot set commands: no existing breakpoint
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_commands_last_breakpoint[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break 4
    Breakpoint 1 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) break 3
    Breakpoint 2 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) clear 2
    Deleted breakpoint 2 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:3
    (Pdb) commands
    (com) p "success"
    (com) end
    (Pdb) continue
    'success'
    > <doctest __main__.test_pdb_commands_last_breakpoint[0]>(4)test_function()
    -> bar = 2
    (Pdb) clear 1
    Deleted breakpoint 1 at <doctest __main__.test_pdb_commands_last_breakpoint[0]>:4
    (Pdb) commands
    *** cannot set commands: no existing breakpoint
    (Pdb) continue

======================================================================
FAIL: test_pdb_continue_in_bottomframe (__main__) [1]
Doctest: __main__.test_pdb_continue_in_bottomframe
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1921, in __main__.test_pdb_continue_in_bottomframe
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'step',
        'next',
        'break 7',
        'continue',
        'next',
        'continue',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(3)test_function()
    -> inst.set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(4)test_function()
    -> inst.botframe = sys._getframe()  # hackery to get the right botframe
    (Pdb) next
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(5)test_function()
    -> print(1)
    (Pdb) break 7
    Breakpoint ... at <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>:7
    (Pdb) continue
    1
    2
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(7)test_function()
    -> print(3)
    (Pdb) next
    3
    > <doctest test.test_pdb.test_pdb_continue_in_bottomframe[0]>(8)test_function()
    -> print(4)
    (Pdb) continue
    4
Got:
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(3)test_function()
    -> inst.set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(4)test_function()
    -> inst.botframe = sys._getframe()  # hackery to get the right botframe
    (Pdb) next
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(5)test_function()
    -> print(1)
    (Pdb) break 7
    Breakpoint 1 at <doctest __main__.test_pdb_continue_in_bottomframe[0]>:7
    (Pdb) continue
    1
    2
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(7)test_function()
    -> print(3)
    (Pdb) next
    3
    > <doctest __main__.test_pdb_continue_in_bottomframe[0]>(8)test_function()
    -> print(4)
    (Pdb) continue
    4

======================================================================
FAIL: test_pdb_displayhook (__main__) [1]
Doctest: __main__.test_pdb_displayhook
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 56, in __main__.test_pdb_displayhook
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'foo',
        'bar',
        'for i in range(5): print(i)',
        'continue',
    ]):
        test_function(1, None)
Expected:
    > <doctest test.test_pdb.test_pdb_displayhook[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) foo
    1
    (Pdb) bar
    (Pdb) for i in range(5): print(i)
    0
    1
    2
    3
    4
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_displayhook[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) foo
    1
    (Pdb) bar
    (Pdb) for i in range(5): print(i)
    0
    *** TypeError: 'NoneType' object is not an iterator
    (Pdb) continue

======================================================================
FAIL: test_pdb_empty_line (__main__) [1]
Doctest: __main__.test_pdb_empty_line
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 694, in __main__.test_pdb_empty_line
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'p x',
        '',  # Should repeat p x
        'n ;; p 0 ;; p x',  # Fill cmdqueue with multiple commands
        '',  # Should still repeat p x
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_empty_line[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p x
    1
    (Pdb)
    1
    (Pdb) n ;; p 0 ;; p x
    0
    1
    > <doctest test.test_pdb.test_pdb_empty_line[0]>(4)test_function()
    -> y = 2
    (Pdb)
    1
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_empty_line[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p x
    1
    (Pdb) 
    1
    (Pdb) n ;; p 0 ;; p x
    0
    1
    > <doctest __main__.test_pdb_empty_line[0]>(4)test_function()
    -> y = 2
    (Pdb) 
    1
    (Pdb) continue

======================================================================
FAIL: test_pdb_f_trace_lines (__main__) [1]
Doctest: __main__.test_pdb_f_trace_lines
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3330, in __main__.test_pdb_f_trace_lines
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'continue'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_f_trace_lines[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_f_trace_lines[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_function_break (__main__) [5]
Doctest: __main__.test_pdb_function_break
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3410, in __main__.test_pdb_function_break
    >>> with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
        'break foo',
        'break bar',
        'break boo',
        'break gen',
        'continue'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_function_break[4]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[0]>:1
    (Pdb) break bar
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[1]>:3
    (Pdb) break boo
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[2]>:4
    (Pdb) break gen
    Breakpoint ... at <doctest test.test_pdb.test_pdb_function_break[3]>:2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_function_break[4]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break foo
    Breakpoint 1 at <doctest __main__.test_pdb_function_break[0]>:1
    (Pdb) break bar
    Breakpoint 2 at <doctest __main__.test_pdb_function_break[1]>:1
    (Pdb) break boo
    Breakpoint 3 at <doctest __main__.test_pdb_function_break[2]>:1
    (Pdb) break gen
    Breakpoint 4 at <doctest __main__.test_pdb_function_break[3]>:1
    (Pdb) continue

======================================================================
FAIL: test_pdb_interact_command (__main__) [3]
Doctest: __main__.test_pdb_interact_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1138, in __main__.test_pdb_interact_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
        'interact',
        'x',
        'g',
        'x = 2',
        'g = 3',
        'dict_g["a"] = True',
        'lst_local.append(x)',
        'exit()',
        'p x',
        'p g',
        'p dict_g',
        'p lst_local',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_interact_command[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) interact
    *pdb interact start*
    ... x
    1
    ... g
    0
    ... x = 2
    ... g = 3
    ... dict_g["a"] = True
    ... lst_local.append(x)
    ... exit()
    *exit from pdb interact command*
    (Pdb) p x
    1
    (Pdb) p g
    0
    (Pdb) p dict_g
    {'a': True}
    (Pdb) p lst_local
    [2]
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_interact_command[2]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) interact
    *pdb interact start*
    >>> x
    1
    >>> g
    0
    >>> x = 2
    >>> g = 3
    >>> dict_g["a"] = True
    >>> lst_local.append(x)
    >>> exit()
    <BLANKLINE>
    *exit from pdb interact command*
    (Pdb) p x
    1
    (Pdb) p g
    0
    (Pdb) p dict_g
    {'a': True}
    (Pdb) p lst_local
    [2]
    (Pdb) continue

======================================================================
FAIL: test_pdb_invalid_arg (__main__) [1]
Doctest: __main__.test_pdb_invalid_arg
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1827, in __main__.test_pdb_invalid_arg
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'a = 3',
        'll 4',
        'step 1',
        'p',
        'enable ',
        'continue'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_invalid_arg[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) a = 3
    *** Invalid argument: = 3
          Usage: a(rgs)
    (Pdb) ll 4
    *** Invalid argument: 4
          Usage: ll | longlist
    (Pdb) step 1
    *** Invalid argument: 1
          Usage: s(tep)
    (Pdb) p
    *** Argument is required for this command
          Usage: p expression
    (Pdb) enable
    *** Argument is required for this command
          Usage: enable bpnumber [bpnumber ...]
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_invalid_arg[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) a = 3
    *** Invalid argument: = 3
          Usage: a(rgs)
    (Pdb) ll 4
    *** Invalid argument: 4
          Usage: ll | longlist
    (Pdb) step 1
    *** Invalid argument: 1
          Usage: s(tep)
    (Pdb) p
    *** Argument is required for this command
          Usage: p expression
    (Pdb) enable 
    *** Argument is required for this command
          Usage: enable bpnumber [bpnumber ...]
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_20766 (__main__) [1]
Doctest: __main__.test_pdb_issue_20766
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2957, in __main__.test_pdb_issue_20766
    >>> with PdbTestInput(['continue',
AssertionError: Failed example:
    with PdbTestInput(['continue',
                       'continue']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 1: <built-in function default_int_handler>
    > <doctest test.test_pdb.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 2: <built-in function default_int_handler>
Got:
    > <doctest __main__.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 1: <built-in function default_int_handler>
    > <doctest __main__.test_pdb_issue_20766[0]>(5)test_function()
    -> sess.set_trace(sys._getframe())
    (Pdb) continue
    pdb 2: <built-in function default_int_handler>

======================================================================
FAIL: test_pdb_issue_gh_101517 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_101517
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3193, in __main__.test_pdb_issue_gh_101517
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'continue'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_101517[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_101517[0]>(5)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_101673 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_101673
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3124, in __main__.test_pdb_issue_gh_101673
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        '!a = 2',
        'll',
        'p a',
        'u',
        'p a',
        'd',
        'p a',
        'continue'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) !a = 2
    (Pdb) ll
      1         def test_function():
      2            a = 1
      3  ->        import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) u
    > <doctest test.test_pdb.test_pdb_issue_gh_101673[1]>(11)<module>()
    -> test_function()
    (Pdb) p a
    *** NameError: name 'a' is not defined
    (Pdb) d
    > <doctest test.test_pdb.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) !a = 2
    (Pdb) ll
      1  	def test_function():
      2  	   a = 1
      3  ->	   import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) u
    > <doctest __main__.test_pdb_issue_gh_101673[1]>(11)<module>()
    -> test_function()
    (Pdb) p a
    *** NameError: name 'a' is not defined
    (Pdb) d
    > <doctest __main__.test_pdb_issue_gh_101673[0]>(3)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p a
    2
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_103225 (__main__) [0]
Doctest: __main__.test_pdb_issue_gh_103225
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3162, in __main__.test_pdb_issue_gh_103225
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'longlist',
        'continue'
    ]):
        a = 1
        import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
        b = 2
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_103225[0]>(6)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) longlist
      1     with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
      2         'longlist',
      3         'continue'
      4     ]):
      5         a = 1
      6 ->      import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      7         b = 2
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_103225[0]>(6)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) longlist
      1  	with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
      2  	    'longlist',
      3  	    'continue'
      4  	]):
      5  	    a = 1
      6  ->	    import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
      7  	    b = 2
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_127321 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_127321
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3226, in __main__.test_pdb_issue_gh_127321
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'continue'
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_127321[0]>(4)test_function()
    -> a = 1
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_127321[0]>(3)test_function()
    -> [1, 2] and pdb_instance.set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_136057 (__main__) [1]
Doctest: __main__.test_pdb_issue_gh_136057
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3244, in __main__.test_pdb_issue_gh_136057
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'next',
        'next',
        'step',
        'continue',
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_136057[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) next
    > <doctest test.test_pdb.test_pdb_issue_gh_136057[0]>(3)test_function()
    -> lst = [i for i in range(10)]
    (Pdb) next
    > <doctest test.test_pdb.test_pdb_issue_gh_136057[0]>(4)test_function()
    -> for i in lst: pass
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_issue_gh_136057[0]>(4)test_function()->None
    -> for i in lst: pass
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_136057[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) next
    > <doctest __main__.test_pdb_issue_gh_136057[0]>(3)test_function()
    -> lst = [i for i in range(10)]
    (Pdb) next
    > <doctest __main__.test_pdb_issue_gh_136057[0]>(4)test_function()
    -> for i in lst: pass
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_issue_gh_136057[0]>(4)test_function()->None
    -> for i in lst: pass
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_65052 (__main__) [2]
Doctest: __main__.test_pdb_issue_gh_65052
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3447, in __main__.test_pdb_issue_gh_65052
    >>> with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS +NORMALIZE_WHITESPACE
        's',
        's',
        'retval',
        'continue',
        'args',
        'display self',
        'display',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(3)__new__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(4)__new__()
    -> return object.__new__(cls)
    (Pdb) s
    --Return--
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(4)__new__()-><A instance at ...>
    -> return object.__new__(cls)
    (Pdb) retval
    *** repr(retval) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue
    > <doctest test.test_pdb.test_pdb_issue_gh_65052[0]>(6)__init__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) args
    self = *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display self
    display self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display
    Currently displaying:
    self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(3)__new__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) s
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(4)__new__()
    -> return object.__new__(cls)
    (Pdb) s
    --Return--
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(4)__new__()-><A instance at 0xX>
    -> return object.__new__(cls)
    (Pdb) retval
    *** repr(retval) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue
    > <doctest __main__.test_pdb_issue_gh_65052[0]>(6)__init__()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) args
    self = *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display self
    display self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) display
    Currently displaying:
    self: *** repr(self) failed: AttributeError: 'A' object has no attribute 'a' ***
    (Pdb) continue

======================================================================
FAIL: test_pdb_issue_gh_80731 (__main__) [0]
Doctest: __main__.test_pdb_issue_gh_80731
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 3272, in __main__.test_pdb_issue_gh_80731
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'import sys',
        'sys.exc_info()',
        'continue'
    ]):
        try:
            raise ValueError('Correct')
        except ValueError:
            import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
Expected:
    > <doctest test.test_pdb.test_pdb_issue_gh_80731[0]>(9)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) import sys
    (Pdb) sys.exc_info()
    (<class 'ValueError'>, ValueError('Correct'), <traceback object at ...>)
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_issue_gh_80731[0]>(9)<module>()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) import sys
    (Pdb) sys.exc_info()
    (<class 'ValueError'>, ValueError('Correct'), <traceback object at 0xX>)
    (Pdb) continue

======================================================================
FAIL: test_pdb_multiline_statement (__main__) [1]
Doctest: __main__.test_pdb_multiline_statement
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2772, in __main__.test_pdb_multiline_statement
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'def f(x):',
        '  return x * 2',
        '',
        'val = 2',
        'if val > 0:',
        '  val = f(val)',
        '',
        '',  # empty line should repeat the multi-line statement
        'val',
        'c'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_multiline_statement[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) def f(x):
    ...     return x * 2
    ...
    (Pdb) val = 2
    (Pdb) if val > 0:
    ...     val = f(val)
    ...
    (Pdb)
    (Pdb) val
    8
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_multiline_statement[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) def f(x):
    ...     return x * 2
    ...   
    *** TypeError: cannot create 'cell' instances
    (Pdb) val = 2
    (Pdb) if val > 0:
    ...     val = f(val)
    ...   
    *** NameError: name 'f' is not defined
    (Pdb) 
    *** NameError: name 'f' is not defined
    (Pdb) val
    2
    (Pdb) c

======================================================================
FAIL: test_pdb_next_command_in_generator_for_loop (__main__) [2]
Doctest: __main__.test_pdb_next_command_in_generator_for_loop
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2654, in __main__.test_pdb_next_command_in_generator_for_loop
    >>> with PdbTestInput(['break test_gen',
AssertionError: Failed example:
    with PdbTestInput(['break test_gen',
                       'continue',
                       'next',
                       'next',
                       'next',
                       'continue']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_next_command_in_generator_for_loop[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break test_gen
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_next_command_in_generator_for_loop[0]>:2
    (Pdb) continue
    > <doctest test.test_pdb.test_pdb_next_command_in_generator_for_loop[0]>(2)test_gen()
    -> yield 0
    (Pdb) next
    value 0
    > <doctest test.test_pdb.test_pdb_next_command_in_generator_for_loop[0]>(3)test_gen()
    -> return 1
    (Pdb) next
    Internal StopIteration: 1
    > <doctest test.test_pdb.test_pdb_next_command_in_generator_for_loop[1]>(3)test_function()
    -> for i in test_gen():
    (Pdb) next
    > <doctest test.test_pdb.test_pdb_next_command_in_generator_for_loop[1]>(5)test_function()
    -> x = 123
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_next_command_in_generator_for_loop[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) break test_gen
    Breakpoint 1 at <doctest __main__.test_pdb_next_command_in_generator_for_loop[0]>:1
    (Pdb) continue
    > <doctest __main__.test_pdb_next_command_in_generator_for_loop[0]>(2)test_gen()
    -> yield 0
    (Pdb) next
    value 0
    > <doctest __main__.test_pdb_next_command_in_generator_for_loop[0]>(3)test_gen()
    -> return 1
    (Pdb) next

======================================================================
FAIL: test_pdb_next_command_subiterator (__main__) [3]
Doctest: __main__.test_pdb_next_command_subiterator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2699, in __main__.test_pdb_next_command_subiterator
    >>> with PdbTestInput(['step',
AssertionError: Failed example:
    with PdbTestInput(['step',
                       'step',
                       'step',
                       'next',
                       'next',
                       'next',
                       'continue']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_next_command_subiterator[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_next_command_subiterator[2]>(3)test_function()
    -> for i in test_gen():
    (Pdb) step
    --Call--
    > <doctest test.test_pdb.test_pdb_next_command_subiterator[1]>(1)test_gen()
    -> def test_gen():
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_next_command_subiterator[1]>(2)test_gen()
    -> x = yield from test_subgenerator()
    (Pdb) next
    value 0
    > <doctest test.test_pdb.test_pdb_next_command_subiterator[1]>(3)test_gen()
    -> return x
    (Pdb) next
    Internal StopIteration: 1
    > <doctest test.test_pdb.test_pdb_next_command_subiterator[2]>(3)test_function()
    -> for i in test_gen():
    (Pdb) next
    > <doctest test.test_pdb.test_pdb_next_command_subiterator[2]>(5)test_function()
    -> x = 123
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_next_command_subiterator[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_next_command_subiterator[2]>(3)test_function()
    -> for i in test_gen():
    (Pdb) step
    --Call--
    > <doctest __main__.test_pdb_next_command_subiterator[1]>(1)test_gen()
    -> def test_gen():
    (Pdb) step
    > <doctest __main__.test_pdb_next_command_subiterator[1]>(2)test_gen()
    -> x = yield from test_subgenerator()
    (Pdb) next
    value 0
    > <doctest __main__.test_pdb_next_command_subiterator[1]>(3)test_gen()
    -> return x
    (Pdb) next

======================================================================
FAIL: test_pdb_pp_repr_exc (__main__) [3]
Doctest: __main__.test_pdb_pp_repr_exc
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 671, in __main__.test_pdb_pp_repr_exc
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'p obj',
        'pp obj',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_pp_repr_exc[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p obj
    *** Exception: repr_exc
    (Pdb) pp obj
    *** Exception: repr_exc
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_pp_repr_exc[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) p obj
    *** Exception: repr_exc
    (Pdb) pp obj
    *** Exception: repr_exc
    (Pdb) continue

======================================================================
FAIL: test_pdb_restart_command (__main__) [1]
Doctest: __main__.test_pdb_restart_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1098, in __main__.test_pdb_restart_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'restart',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_restart_command[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False, mode='inline').set_trace()
    (Pdb) restart
    *** run/restart command is disabled when pdb is running in inline mode.
    Use the command line interface to enable restarting your program
    e.g. "python -m pdb myscript.py"
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_restart_command[0]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False, mode='inline').set_trace()
    (Pdb) restart
    *** run/restart command is disabled when pdb is running in inline mode.
    Use the command line interface to enable restarting your program
    e.g. "python -m pdb myscript.py"
    (Pdb) continue

======================================================================
FAIL: test_pdb_return_to_different_file (__main__) [3]
Doctest: __main__.test_pdb_return_to_different_file
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1758, in __main__.test_pdb_return_to_different_file
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
        'b A.__repr__',
        'continue',
        'return',
        'next',
        'return',
        'return',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_return_to_different_file[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b A.__repr__
    Breakpoint 1 at <doctest test.test_pdb.test_pdb_return_to_different_file[1]>:3
    (Pdb) continue
    > <doctest test.test_pdb.test_pdb_return_to_different_file[1]>(3)__repr__()
    -> return 'A'
    (Pdb) return
    --Return--
    > <doctest test.test_pdb.test_pdb_return_to_different_file[1]>(3)__repr__()->'A'
    -> return 'A'
    (Pdb) next
    > ...pprint.py..._safe_repr()
    -> return rep,...
    (Pdb) return
    --Return--
    > ...pprint.py..._safe_repr()->('A'...)
    -> return rep,...
    (Pdb) return
    --Return--
    > ...pprint.py...format()->('A'...)
    -> return...
    (Pdb) continue
    A
Got:
    > <doctest __main__.test_pdb_return_to_different_file[2]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) b A.__repr__
    Breakpoint 1 at <doctest __main__.test_pdb_return_to_different_file[1]>:2
    (Pdb) continue
    > <doctest __main__.test_pdb_return_to_different_file[1]>(3)__repr__()
    -> return 'A'
    (Pdb) return
    --Return--
    > <doctest __main__.test_pdb_return_to_different_file[1]>(3)__repr__()->'A'
    -> return 'A'
    (Pdb) next
    > /pkg/store/python-0/lib/pprint.py(920)_safe_repr()
    -> return rep, (rep and not rep.startswith('<')), False
    (Pdb) return
    --Return--
    > /pkg/store/python-0/lib/pprint.py(920)_safe_repr()->('A', True, False)
    -> return rep, (rep and not rep.startswith('<')), False
    (Pdb) return
    --Return--
    > /pkg/store/python-0/lib/pprint.py(638)format()->('A', True, False)
    -> return self._safe_repr(object, context, maxlevels, level)
    (Pdb) continue
    A

======================================================================
FAIL: test_pdb_run_with_incorrect_argument (__main__) [1]
Doctest: __main__.test_pdb_run_with_incorrect_argument
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1964, in __main__.test_pdb_run_with_incorrect_argument
    >>> with pti:
AssertionError: Failed example:
    with pti:
        pdb_invoke('run', lambda x: x)
Expected:
    Traceback (most recent call last):
    TypeError: exec() arg 1 must be a string, bytes or code object
Got:
    Traceback (most recent call last):
      File "<doctest __main__.test_pdb_run_with_incorrect_argument[1]>", line 2, in <module>
        pdb_invoke('run', lambda x: x)
      File "/tmp/test_pdb.py", line 1957, in pdb_invoke
        getattr(pdb.Pdb(nosigint=True, readrc=False), method)(arg)
      File "/pkg/store/python-0/lib/bdb.py", line 904, in run
        exec(cmd, globals, locals)
    TypeError: source must be a string, bytes or a code object: function

======================================================================
FAIL: test_pdb_run_with_incorrect_argument (__main__) [2]
Doctest: __main__.test_pdb_run_with_incorrect_argument
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1969, in __main__.test_pdb_run_with_incorrect_argument
    >>> with pti:
AssertionError: Failed example:
    with pti:
        pdb_invoke('runeval', lambda x: x)
Expected:
    Traceback (most recent call last):
    TypeError: eval() arg 1 must be a string, bytes or code object
Got:
    Traceback (most recent call last):
      File "<doctest __main__.test_pdb_run_with_incorrect_argument[2]>", line 2, in <module>
        pdb_invoke('runeval', lambda x: x)
      File "/tmp/test_pdb.py", line 1957, in pdb_invoke
        getattr(pdb.Pdb(nosigint=True, readrc=False), method)(arg)
      File "/pkg/store/python-0/lib/bdb.py", line 924, in runeval
        return eval(expr, globals, locals)
    TypeError: source must be a string, bytes or a code object: function

======================================================================
FAIL: test_pdb_show_attribute_and_item (__main__) [1]
Doctest: __main__.test_pdb_show_attribute_and_item
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2871, in __main__.test_pdb_show_attribute_and_item
    >>> with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +NORMALIZE_WHITESPACE
        'c["a"]',
        'c.get("a")',
        'n(1)',
        'j=1',
        'j+1',
        'r"a"',
        'next(iter([1]))',
        'list((0, 1))',
        'c'
    ]):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_show_attribute_and_item[0]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) c["a"]
    1
    (Pdb) c.get("a")
    1
    (Pdb) n(1)
    1
    (Pdb) j=1
    (Pdb) j+1
    2
    (Pdb) r"a"
    'a'
    (Pdb) next(iter([1]))
    1
    (Pdb) list((0, 1))
    [0, 1]
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_show_attribute_and_item[0]>(4)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) c["a"]
    1
    (Pdb) c.get("a")
    1
    (Pdb) n(1)
    1
    (Pdb) j=1
    (Pdb) j+1
    2
    (Pdb) r"a"
    'a'
    (Pdb) next(iter([1]))
    1
    (Pdb) list((0, 1))
    [0, 1]
    (Pdb) c

======================================================================
FAIL: test_pdb_skip_modules (__main__) [1]
Doctest: __main__.test_pdb_skip_modules
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1803, in __main__.test_pdb_skip_modules
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'step',
        'step',
        'continue',
    ]):
        skip_module()
Expected:
    > <doctest test.test_pdb.test_pdb_skip_modules[0]>(3)skip_module()
    -> import pdb; pdb.Pdb(skip=['stri*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules[0]>(4)skip_module()
    -> string.capwords('FOO')
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_skip_modules[0]>(4)skip_module()->None
    -> string.capwords('FOO')
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_skip_modules[0]>(3)skip_module()
    -> import pdb; pdb.Pdb(skip=['stri*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules[0]>(4)skip_module()
    -> string.capwords('FOO')
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_skip_modules[0]>(4)skip_module()->None
    -> string.capwords('FOO')
    (Pdb) continue

======================================================================
FAIL: test_pdb_skip_modules_with_callback (__main__) [1]
Doctest: __main__.test_pdb_skip_modules_with_callback
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1871, in __main__.test_pdb_skip_modules_with_callback
    >>> with PdbTestInput([
AssertionError: Failed example:
    with PdbTestInput([
        'step',
        'step',
        'step',
        'step',
        'step',
        'step',
        'continue',
    ]):
        skip_module()
        pass  # provides something to "step" to
Expected:
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(4)skip_module()
    -> import pdb; pdb.Pdb(skip=['module_to_skip*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(5)skip_module()
    -> mod.foo_pony(callback)
    (Pdb) step
    --Call--
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(2)callback()
    -> def callback():
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(3)callback()
    -> return None
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(3)callback()->None
    -> return None
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[0]>(5)skip_module()->None
    -> mod.foo_pony(callback)
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_skip_modules_with_callback[1]>(11)<module>()
    -> pass  # provides something to "step" to
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(4)skip_module()
    -> import pdb; pdb.Pdb(skip=['module_to_skip*'], nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(5)skip_module()
    -> mod.foo_pony(callback)
    (Pdb) step
    --Call--
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(2)callback()
    -> def callback():
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(3)callback()
    -> return None
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(3)callback()->None
    -> return None
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_skip_modules_with_callback[0]>(5)skip_module()->None
    -> mod.foo_pony(callback)
    (Pdb) step
    > <doctest __main__.test_pdb_skip_modules_with_callback[1]>(11)<module>()
    -> pass  # provides something to "step" to
    (Pdb) continue

======================================================================
FAIL: test_pdb_until_command_for_generator (__main__) [2]
Doctest: __main__.test_pdb_until_command_for_generator
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2559, in __main__.test_pdb_until_command_for_generator
    >>> with PdbTestInput(['step',
AssertionError: Failed example:
    with PdbTestInput(['step',
                       'step',
                       'until 4',
                       'step',
                       'step',
                       'continue']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_until_command_for_generator[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_until_command_for_generator[1]>(3)test_function()
    -> for i in test_gen():
    (Pdb) step
    --Call--
    > <doctest test.test_pdb.test_pdb_until_command_for_generator[0]>(1)test_gen()
    -> def test_gen():
    (Pdb) until 4
    0
    1
    > <doctest test.test_pdb.test_pdb_until_command_for_generator[0]>(4)test_gen()
    -> yield 2
    (Pdb) step
    --Return--
    > <doctest test.test_pdb.test_pdb_until_command_for_generator[0]>(4)test_gen()->2
    -> yield 2
    (Pdb) step
    > <doctest test.test_pdb.test_pdb_until_command_for_generator[1]>(4)test_function()
    -> print(i)
    (Pdb) continue
    2
    finished
Got:
    > <doctest __main__.test_pdb_until_command_for_generator[1]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) step
    > <doctest __main__.test_pdb_until_command_for_generator[1]>(3)test_function()
    -> for i in test_gen():
    (Pdb) step
    --Call--
    > <doctest __main__.test_pdb_until_command_for_generator[0]>(1)test_gen()
    -> def test_gen():
    (Pdb) until 4
    0
    1
    > <doctest __main__.test_pdb_until_command_for_generator[0]>(4)test_gen()
    -> yield 2
    (Pdb) step
    --Return--
    > <doctest __main__.test_pdb_until_command_for_generator[0]>(4)test_gen()->2
    -> yield 2
    (Pdb) step
    > <doctest __main__.test_pdb_until_command_for_generator[1]>(4)test_function()
    -> print(i)
    (Pdb) continue
    2
    finished

======================================================================
FAIL: test_pdb_whatis_command (__main__) [4]
Doctest: __main__.test_pdb_whatis_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 837, in __main__.test_pdb_whatis_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS, +NORMALIZE_WHITESPACE
       'whatis myvar',
       'whatis myfunc',
       'whatis MyClass',
       'whatis MyClass()',
       'whatis MyClass.mymethod',
       'whatis MyClass().mymethod',
       'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_whatis_command[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) whatis myvar
    <class 'tuple'>
    (Pdb) whatis myfunc
    Function myfunc
    (Pdb) whatis MyClass
    Class test.test_pdb.MyClass
    (Pdb) whatis MyClass()
    <class 'test.test_pdb.MyClass'>
    (Pdb) whatis MyClass.mymethod
    Function mymethod
    (Pdb) whatis MyClass().mymethod
    Method mymethod
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_whatis_command[3]>(2)test_function()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) whatis myvar
    <class 'tuple'>
    (Pdb) whatis myfunc
    Function myfunc
    (Pdb) whatis MyClass
    Class __main__.MyClass
    (Pdb) whatis MyClass()
    <class '__main__.MyClass'>
    (Pdb) whatis MyClass.mymethod
    Function mymethod
    (Pdb) whatis MyClass().mymethod
    Method mymethod
    (Pdb) continue

======================================================================
FAIL: test_pdb_where_command (__main__) [3]
Doctest: __main__.test_pdb_where_command
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 1012, in __main__.test_pdb_where_command
    >>> with PdbTestInput([  # doctest: +ELLIPSIS
AssertionError: Failed example:
    with PdbTestInput([  # doctest: +ELLIPSIS
        'w',
        'where',
        'w 1',
        'w invalid',
        'u',
        'w',
        'w 0',
        'w 100',
        'w -100',
        'continue',
    ]):
       test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) where
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 1
    > <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w invalid
    *** Invalid count (invalid)
    (Pdb) u
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 0
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w 100
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w -100
    ...
      <doctest test.test_pdb.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest test.test_pdb.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest test.test_pdb.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest test.test_pdb.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue
Got:
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) where
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
      <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 1
    > <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w invalid
    *** Invalid count (invalid)
    (Pdb) u
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w 0
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
    (Pdb) w 100
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) w -100
      /tmp/test_pdb.py(5366)<module>()->None
    -> unittest.main()
      /pkg/store/python-0/lib/unittest/main.py(104)__init__()
    -> self.runTests()
      /pkg/store/python-0/lib/unittest/main.py(273)runTests()
    -> self.result = testRunner.run(self.test)
      /pkg/store/python-0/lib/unittest/runner.py(259)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/suite.py(84)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/unittest/suite.py(122)run()
    -> test(result)
      /pkg/store/python-0/lib/unittest/case.py(747)__call__()
    -> return self.run(*args, **kwds)
      /pkg/store/python-0/lib/doctest.py(2398)run()
    -> return super().run(result)
      /pkg/store/python-0/lib/unittest/case.py(691)run()
    -> self._callTestMethod(testMethod)
      /pkg/store/python-0/lib/unittest/case.py(637)_callTestMethod()
    -> result = method()
      /pkg/store/python-0/lib/doctest.py(2428)runTest()
    -> results = runner.run(test, out=out, clear_globs=False)
      /pkg/store/python-0/lib/doctest.py(1604)run()
    -> return self.__run(test, compileflags, out)
      /pkg/store/python-0/lib/doctest.py(1440)__run()
    -> exec(compile(example.source, filename, "single",
      <doctest __main__.test_pdb_where_command[3]>(13)<module>()
    -> test_function()
      <doctest __main__.test_pdb_where_command[2]>(2)test_function()
    -> f()
    > <doctest __main__.test_pdb_where_command[1]>(2)f()
    -> g()
      <doctest __main__.test_pdb_where_command[0]>(2)g()
    -> import pdb; pdb.Pdb(nosigint=True, readrc=False).set_trace()
    (Pdb) continue

======================================================================
FAIL: test_pdb_with_inline_breakpoint (__main__) [1]
Doctest: __main__.test_pdb_with_inline_breakpoint
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pdb.py", line 2916, in __main__.test_pdb_with_inline_breakpoint
    >>> with PdbTestInput(['display x',
AssertionError: Failed example:
    with PdbTestInput(['display x',
                       'n',
                       'n',
                       'n',
                       'n',
                       'undisplay',
                       'c']):
        test_function()
Expected:
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(3)test_function()
    -> import pdb; pdb.Pdb().set_trace()
    (Pdb) display x
    display x: 1
    (Pdb) n
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(5)test_function()
    -> x = 2
    (Pdb) n
    --Return--
    > <doctest test.test_pdb.test_pdb_with_inline_breakpoint[0]>(5)test_function()->None
    -> x = 2
    display x: 2  [old: 1]
    (Pdb) undisplay
    (Pdb) c
Got:
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(3)test_function()
    -> import pdb; pdb.Pdb().set_trace()
    (Pdb) display x
    display x: 1
    (Pdb) n
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(4)test_function()
    -> original_pdb_settrace()
    (Pdb) n
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(5)test_function()
    -> x = 2
    (Pdb) n
    --Return--
    > <doctest __main__.test_pdb_with_inline_breakpoint[0]>(5)test_function()->None
    -> x = 2
    display x: 2  [old: 1]
    (Pdb) undisplay
    (Pdb) c

----------------------------------------------------------------------
Ran 223 tests in Ns

FAILED (failures=84, errors=124, skipped=17)
