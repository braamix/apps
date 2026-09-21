sss..s.ssFsFsss.s.ssssFs.s.sssssssssssss....ssssss.ss...ss.ssssssssssFsssssssEssssssssssFFsss.ss..FssssssssssssssssssssEEEEE.EEEEEEEEEEEEssssFsEEEEEEE.EEFEEEEEEEEEEssssFsEEsssssssssssssssssss
======================================================================
ERROR: test_rtld_constants (__main__.PosixTester.test_rtld_constants)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 1530, in test_rtld_constants
    posix.RTLD_LAZY
AttributeError: module 'posix' has no attribute 'RTLD_LAZY'

======================================================================
ERROR: test_bad_file_actions (__main__.TestPosixSpawn.test_bad_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2255, in test_bad_file_actions
    self.spawn_func(args[0], args, os.environ,
NotImplementedError: an open file_action cannot be had without fork

======================================================================
ERROR: test_close_file (__main__.TestPosixSpawn.test_close_file)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2307, in test_close_file
    pid = self.spawn_func(args[0], args, os.environ,
NotImplementedError: closing 0, 1 or 2 cannot be had without fork

======================================================================
ERROR: test_dup2 (__main__.TestPosixSpawn.test_dup2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2328, in test_dup2
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_empty_file_actions (__main__.TestPosixSpawn.test_empty_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2040, in test_empty_file_actions
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_multiple_file_actions (__main__.TestPosixSpawn.test_multiple_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2240, in test_multiple_file_actions
    pid = self.spawn_func(self.NOOP_PROGRAM[0],
NotImplementedError: an open file_action cannot be had without fork

======================================================================
ERROR: test_none_file_actions (__main__.TestPosixSpawn.test_none_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2031, in test_none_file_actions
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_open_file (__main__.TestPosixSpawn.test_open_file)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2288, in test_open_file
    pid = self.spawn_func(args[0], args, os.environ,
NotImplementedError: an open file_action cannot be had without fork

======================================================================
ERROR: test_resetids (__main__.TestPosixSpawn.test_resetids)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2052, in test_resetids
    pid = self.spawn_func(
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_resetids_explicit_default (__main__.TestPosixSpawn.test_resetids_explicit_default)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2049, in test_resetids_explicit_default
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_returns_pid (__main__.TestPosixSpawn.test_returns_pid)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 1982, in test_returns_pid
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_scheduler_allow_none (__main__.TestPosixSpawn.test_scheduler_allow_none)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2177, in test_scheduler_allow_none
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_scheduler_wrong_type (__main__.TestPosixSpawn.test_scheduler_wrong_type) (scheduler=<object object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_posix.py", line 2186, in test_scheduler_wrong_type
    self.spawn_func(path, args, os.environ, scheduler=scheduler)
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_scheduler_wrong_type (__main__.TestPosixSpawn.test_scheduler_wrong_type) (scheduler=1)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_posix.py", line 2186, in test_scheduler_wrong_type
    self.spawn_func(path, args, os.environ, scheduler=scheduler)
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_scheduler_wrong_type (__main__.TestPosixSpawn.test_scheduler_wrong_type) (scheduler=[1, 2])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_posix.py", line 2186, in test_scheduler_wrong_type
    self.spawn_func(path, args, os.environ, scheduler=scheduler)
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_setpgroup (__main__.TestPosixSpawn.test_setpgroup)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2065, in test_setpgroup
    setpgroup=os.getpgrp()
AttributeError: module 'os' has no attribute 'getpgrp'

======================================================================
ERROR: test_setpgroup_allow_none (__main__.TestPosixSpawn.test_setpgroup_allow_none)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2072, in test_setpgroup_allow_none
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_setpgroup_wrong_type (__main__.TestPosixSpawn.test_setpgroup_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2076, in test_setpgroup_wrong_type
    self.spawn_func(sys.executable,
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_setsigmask_wrong_type (__main__.TestPosixSpawn.test_setsigmask_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2101, in test_setsigmask_wrong_type
    self.spawn_func(sys.executable,
NotImplementedError: a signal mask cannot be had without fork

======================================================================
ERROR: test_specify_environment (__main__.TestPosixSpawn.test_specify_environment)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2019, in test_specify_environment
    {**os.environ, 'foo': 'bar'})
TypeError: '_Environ' object is not a mapping

======================================================================
ERROR: test_bad_file_actions (__main__.TestPosixSpawnP.test_bad_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2255, in test_bad_file_actions
    self.spawn_func(args[0], args, os.environ,
NotImplementedError: an open file_action cannot be had without fork

======================================================================
ERROR: test_close_file (__main__.TestPosixSpawnP.test_close_file)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2307, in test_close_file
    pid = self.spawn_func(args[0], args, os.environ,
NotImplementedError: closing 0, 1 or 2 cannot be had without fork

======================================================================
ERROR: test_dup2 (__main__.TestPosixSpawnP.test_dup2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2328, in test_dup2
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_empty_file_actions (__main__.TestPosixSpawnP.test_empty_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2040, in test_empty_file_actions
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_multiple_file_actions (__main__.TestPosixSpawnP.test_multiple_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2240, in test_multiple_file_actions
    pid = self.spawn_func(self.NOOP_PROGRAM[0],
NotImplementedError: an open file_action cannot be had without fork

======================================================================
ERROR: test_none_file_actions (__main__.TestPosixSpawnP.test_none_file_actions)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2031, in test_none_file_actions
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_open_file (__main__.TestPosixSpawnP.test_open_file)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2288, in test_open_file
    pid = self.spawn_func(args[0], args, os.environ,
NotImplementedError: an open file_action cannot be had without fork

======================================================================
ERROR: test_resetids (__main__.TestPosixSpawnP.test_resetids)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2052, in test_resetids
    pid = self.spawn_func(
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_resetids_explicit_default (__main__.TestPosixSpawnP.test_resetids_explicit_default)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2049, in test_resetids_explicit_default
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_returns_pid (__main__.TestPosixSpawnP.test_returns_pid)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 1982, in test_returns_pid
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_scheduler_allow_none (__main__.TestPosixSpawnP.test_scheduler_allow_none)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2177, in test_scheduler_allow_none
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_scheduler_wrong_type (__main__.TestPosixSpawnP.test_scheduler_wrong_type) (scheduler=<object object at 0xX>)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_posix.py", line 2186, in test_scheduler_wrong_type
    self.spawn_func(path, args, os.environ, scheduler=scheduler)
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_scheduler_wrong_type (__main__.TestPosixSpawnP.test_scheduler_wrong_type) (scheduler=1)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_posix.py", line 2186, in test_scheduler_wrong_type
    self.spawn_func(path, args, os.environ, scheduler=scheduler)
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_scheduler_wrong_type (__main__.TestPosixSpawnP.test_scheduler_wrong_type) (scheduler=[1, 2])
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test/support/__init__.py", line 282, in wrapper
    func(self, *args, **kwargs, **subtest_kwargs)
  File "/tmp/test_posix.py", line 2186, in test_scheduler_wrong_type
    self.spawn_func(path, args, os.environ, scheduler=scheduler)
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_setpgroup (__main__.TestPosixSpawnP.test_setpgroup)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2065, in test_setpgroup
    setpgroup=os.getpgrp()
AttributeError: module 'os' has no attribute 'getpgrp'

======================================================================
ERROR: test_setpgroup_allow_none (__main__.TestPosixSpawnP.test_setpgroup_allow_none)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2072, in test_setpgroup_allow_none
    support.wait_process(pid, exitcode=0)
AttributeError: module 'test.support' has no attribute 'wait_process'

======================================================================
ERROR: test_setpgroup_wrong_type (__main__.TestPosixSpawnP.test_setpgroup_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2076, in test_setpgroup_wrong_type
    self.spawn_func(sys.executable,
NotImplementedError: a process group, session, id or scheduler cannot be had without fork

======================================================================
ERROR: test_setsigmask_wrong_type (__main__.TestPosixSpawnP.test_setsigmask_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2101, in test_setsigmask_wrong_type
    self.spawn_func(sys.executable,
NotImplementedError: a signal mask cannot be had without fork

======================================================================
ERROR: test_specify_environment (__main__.TestPosixSpawnP.test_specify_environment)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2019, in test_specify_environment
    {**os.environ, 'foo': 'bar'})
TypeError: '_Environ' object is not a mapping

======================================================================
FAIL: test_chmod_dir_symlink (__main__.PosixTester.test_chmod_dir_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 1179, in test_chmod_dir_symlink
    self.check_chmod_link(posix.chmod, target, link)
  File "/tmp/test_posix.py", line 1137, in check_chmod_link
    self.assertEqual(os.stat(target).st_mode, new_mode)
AssertionError: 16877 != 16749

======================================================================
FAIL: test_chmod_file_symlink (__main__.PosixTester.test_chmod_file_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 1167, in test_chmod_file_symlink
    self.check_chmod_link(posix.chmod, target, link)
  File "/tmp/test_posix.py", line 1137, in check_chmod_link
    self.assertEqual(os.stat(target).st_mode, new_mode)
AssertionError: 33188 != 33060

======================================================================
FAIL: test_fstat (__main__.PosixTester.test_fstat)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 685, in test_fstat
    self.assertRaisesRegex(TypeError,
AssertionError: "should be string, bytes, os.PathLike or integer, not" does not match "stat: path should be string, bytes or os.PathLike, not float"

======================================================================
FAIL: test_putenv (__main__.PosixTester.test_putenv)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 1281, in test_putenv
    with self.assertRaises(ValueError):
AssertionError: ValueError not raised

======================================================================
FAIL: test_stat (__main__.PosixTester.test_stat)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 722, in test_stat
    self.check_statlike_path(posix.stat)
  File "/tmp/test_posix.py", line 708, in check_statlike_path
    self.assertRaisesRegex(TypeError,
AssertionError: "should be string, bytes, os.PathLike or integer, not" does not match "stat: path should be string, bytes or os.PathLike, not bytearray"

======================================================================
FAIL: test_stat_fd_zero_follow_symlinks (__main__.PosixTester.test_stat_fd_zero_follow_symlinks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 696, in test_stat_fd_zero_follow_symlinks
    with self.assertRaisesRegex(ValueError,
AssertionError: ValueError not raised

======================================================================
FAIL: test_utime (__main__.PosixTester.test_utime)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 1068, in test_utime
    self.assertRaises(TypeError, posix.utime,
AssertionError: TypeError not raised by utime

======================================================================
FAIL: test_setsigdef_wrong_type (__main__.TestPosixSpawn.test_setsigdef_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2161, in test_setsigdef_wrong_type
    with self.assertRaises(TypeError):
AssertionError: TypeError not raised

======================================================================
FAIL: test_posix_spawnp (__main__.TestPosixSpawnP.test_posix_spawnp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2374, in test_posix_spawnp
    assert_python_ok(*args, PATH=path)
  File "/tmp/test/support/script_helper.py", line 101, in assert_python_ok
    return _assert_python(True, *args, **env_vars)
  File "/tmp/test/support/script_helper.py", line 96, in _assert_python
    res.fail(cmd_line)
  File "/tmp/test/support/script_helper.py", line 37, in fail
    raise AssertionError(
AssertionError: Process return code is 1
command line: ['/bin/py', '-X', 'faulthandler', '-c', "\nimport os\nfrom test import support\n\nargs = ('posix_spawnp_test_program.exe', '-I', '-S', '-c', 'pass')\npid = os.posix_spawnp(args[0], args, os.environ)\n\nsupport.wait_process(pid, exitcode=0)\n"]

stdout:
---

---

stderr:
---
Traceback (most recent call last):
  File "<string>", line 3, in <module>
ModuleNotFoundError: No module named 'test'
---

======================================================================
FAIL: test_setsigdef_wrong_type (__main__.TestPosixSpawnP.test_setsigdef_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_posix.py", line 2161, in test_setsigdef_wrong_type
    with self.assertRaises(TypeError):
AssertionError: TypeError not raised

----------------------------------------------------------------------
Ran 187 tests in Ns

FAILED (failures=10, errors=39, skipped=121)
