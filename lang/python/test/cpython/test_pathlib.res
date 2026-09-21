..s..s.....s..ss......s.s.s.......s............s..ss.s....s...s.s..ss.s.s....FF....F..Fssssss.s.....s..F...s....F.s.E.F......sE.......FFFF.Fs..............................s..ssss..s....s.s...E....F.FFFs...s.......ssFs.s....s.s..s..s.....s..ss......s.s.s.......s............s..ss.s....s...s.s..ss.s.s....FF....F..Fssssss.s.....s..F...s....F.s.E.F......sE.......FFFF.Fs..............................s..ssss..s....s.s....E....F.FFFs...s.......ssFs.s....s.s..s.E.....s.....s..ss......s.s.s.......s............s..ss.s....s...s.s..ss.s.s....FF....F..Fssssss.s.....s..F...s....F.s.E.F......sE.......FFFF.Fs..............................s..ssss..s....s.s....E....F.FFFs...s.......ssFs.s....s.s..s.....s.....s....s...s.s...s.s....s..s..s...s...s...ss..s.s.....s.....s....s...s.s...s.s....s..s..s...s....s...ss..s.s.....s.....s....s...s.s...s.s....s..s..s...s....s...ss..s.s...ss..............s.....s......s.....s.s......s............sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss
======================================================================
ERROR: test_is_socket_true (__main__.PathSubclassTest.test_is_socket_true)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2865, in test_is_socket_true
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_matches_writablepath_docstrings (__main__.PathSubclassTest.test_matches_writablepath_docstrings)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 1189, in test_matches_writablepath_docstrings
    path_names = {name for name in dir(pathlib.types._WritablePath) if name[0] != '_'}
AttributeError: module 'pathlib' has no attribute 'types'

======================================================================
ERROR: test_resolve_common (__main__.PathSubclassTest.test_resolve_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 1888, in test_resolve_common
    d = self.tempdir()
  File "/tmp/test_pathlib.py", line 1183, in tempdir
    d = os_helper._longpath(tempfile.mkdtemp(suffix='-dirD',
AttributeError: module 'test.support.os_helper' has no attribute '_longpath'

======================================================================
ERROR: test_is_socket_true (__main__.PathTest.test_is_socket_true)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2865, in test_is_socket_true
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_matches_writablepath_docstrings (__main__.PathTest.test_matches_writablepath_docstrings)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 1189, in test_matches_writablepath_docstrings
    path_names = {name for name in dir(pathlib.types._WritablePath) if name[0] != '_'}
AttributeError: module 'pathlib' has no attribute 'types'

======================================================================
ERROR: test_resolve_common (__main__.PathTest.test_resolve_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 1888, in test_resolve_common
    d = self.tempdir()
  File "/tmp/test_pathlib.py", line 1183, in tempdir
    d = os_helper._longpath(tempfile.mkdtemp(suffix='-dirD',
AttributeError: module 'test.support.os_helper' has no attribute '_longpath'

======================================================================
ERROR: test_walk_bad_dir (__main__.PathWalkTest.test_walk_bad_dir)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3652, in test_walk_bad_dir
    path1.rename(path1new)
  File "/pkg/store/python-0/lib/pathlib/__init__.py", line 1279, in rename
    os.rename(self, target)
OSError: [Errno 18] Invalid cross-device link: '/home/@test_N_tmpæ/TEST1/SUB1' -> '/home/@test_N_tmpæ/TEST1/SUB1.new'

======================================================================
ERROR: test_is_socket_true (__main__.PosixPathTest.test_is_socket_true)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2865, in test_is_socket_true
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_matches_writablepath_docstrings (__main__.PosixPathTest.test_matches_writablepath_docstrings)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 1189, in test_matches_writablepath_docstrings
    path_names = {name for name in dir(pathlib.types._WritablePath) if name[0] != '_'}
AttributeError: module 'pathlib' has no attribute 'types'

======================================================================
ERROR: test_resolve_common (__main__.PosixPathTest.test_resolve_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 1888, in test_resolve_common
    d = self.tempdir()
  File "/tmp/test_pathlib.py", line 1183, in tempdir
    d = os_helper._longpath(tempfile.mkdtemp(suffix='-dirD',
AttributeError: module 'test.support.os_helper' has no attribute '_longpath'

======================================================================
FAIL: test_glob_dot (__main__.PathSubclassTest.test_glob_dot)
----------------------------------------------------------------------
AssertionError: Items in the first set but not the second:
cls('dirD')
cls('fileC')
cls('novel.txt')
Items in the second set but not the first:
cls('fileC')
cls('novel.txt')
cls('dirD')

======================================================================
FAIL: test_glob_dotdot (__main__.PathSubclassTest.test_glob_dotdot)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3054, in test_glob_dotdot
    self.assertEqual(set(p.glob("..")), { P(self.base, "..") })
AssertionError: Items in the first set but not the second:
cls('/home/@test_N_tmpæ/..')
Items in the second set but not the first:
cls('/home/@test_N_tmpæ/..')

======================================================================
FAIL: test_glob_pathlike (__main__.PathSubclassTest.test_glob_pathlike)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3020, in test_glob_pathlike
    self.assertEqual(expect, set(p.glob(P(pattern))))
AssertionError: Items in the first set but not the second:
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirC/fileC')
Items in the second set but not the first:
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirC/fileC')

======================================================================
FAIL: test_glob_recurse_symlinks_common (__main__.PathSubclassTest.test_glob_recurse_symlinks_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3097, in test_glob_recurse_symlinks_common
    _check(p, "dir*/file*", ["dirB/fileB", "dirC/fileC"])
  File "/tmp/test_pathlib.py", line 3093, in _check
    self.assertEqual(actual, { P(self.base, q) for q in expected })
AssertionError: Items in the first set but not the second:
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirC/fileC')
Items in the second set but not the first:
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirC/fileC')

======================================================================
FAIL: test_is_char_device_true (__main__.PathSubclassTest.test_is_char_device_true)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2903, in test_is_char_device_true
    self.assertTrue(P.is_char_device())
AssertionError: False is not true

======================================================================
FAIL: test_is_mount (__main__.PathSubclassTest.test_is_mount)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2921, in test_is_mount
    self.assertTrue(R.is_mount())
AssertionError: False is not true

======================================================================
FAIL: test_iterdir_symlink (__main__.PathSubclassTest.test_iterdir_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2956, in test_iterdir_symlink
    self.assertEqual(paths, expected)
AssertionError: Items in the first set but not the second:
cls('/home/@test_N_tmpæ/linkB/fileB')
cls('/home/@test_N_tmpæ/linkB/linkD')
Items in the second set but not the first:
cls('/home/@test_N_tmpæ/linkB/fileB')
cls('/home/@test_N_tmpæ/linkB/linkD')

======================================================================
FAIL: test_mkdir_parent_mode_deep_hierarchy (__main__.PathSubclassTest.test_mkdir_parent_mode_deep_hierarchy)
----------------------------------------------------------------------
AssertionError: 493 != 448

======================================================================
FAIL: test_mkdir_parent_mode_same_as_mode (__main__.PathSubclassTest.test_mkdir_parent_mode_same_as_mode)
----------------------------------------------------------------------
AssertionError: 493 != 453

======================================================================
FAIL: test_mkdir_parents (__main__.PathSubclassTest.test_mkdir_parents)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2392, in test_mkdir_parents
    self.assertEqual(stat.S_IMODE(p.stat().st_mode), 0o7555 & mode)
AssertionError: 493 != 365

======================================================================
FAIL: test_mkdir_parents_umask (__main__.PathSubclassTest.test_mkdir_parents_umask)
----------------------------------------------------------------------
AssertionError: 493 != 509

======================================================================
FAIL: test_mkdir_with_parent_mode (__main__.PathSubclassTest.test_mkdir_with_parent_mode)
----------------------------------------------------------------------
AssertionError: 493 != 488

======================================================================
FAIL: test_rglob_pathlike (__main__.PathSubclassTest.test_rglob_pathlike)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3085, in test_rglob_pathlike
    self.assertEqual(expect, set(p.rglob(P(pattern))))
AssertionError: Items in the first set but not the second:
cls('/home/@test_N_tmpæ/dirC/fileC')
cls('/home/@test_N_tmpæ/dirC/dirD/fileD')
Items in the second set but not the first:
cls('/home/@test_N_tmpæ/dirC/fileC')
cls('/home/@test_N_tmpæ/dirC/dirD/fileD')

======================================================================
FAIL: test_rglob_recurse_symlinks_common (__main__.PathSubclassTest.test_rglob_recurse_symlinks_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3133, in test_rglob_recurse_symlinks_common
    _check(p, "fileB", ["dirB/fileB", "dirA/linkC/fileB", "linkB/fileB",
  File "/tmp/test_pathlib.py", line 3130, in _check
    self.assertEqual(actual, { P(self.base, q) for q in expected })
AssertionError: Items in the first set but not the second:
cls('/home/@test_N_tmpæ/linkB/fileB')
cls('/home/@test_N_tmpæ/linkB/linkD/fileB')
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirB/linkD/fileB')
cls('/home/@test_N_tmpæ/dirA/linkC/fileB')
cls('/home/@test_N_tmpæ/dirA/linkC/linkD/fileB')
Items in the second set but not the first:
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirA/linkC/fileB')
cls('/home/@test_N_tmpæ/linkB/fileB')
cls('/home/@test_N_tmpæ/dirA/linkC/linkD/fileB')
cls('/home/@test_N_tmpæ/dirB/linkD/fileB')
cls('/home/@test_N_tmpæ/linkB/linkD/fileB')

======================================================================
FAIL: test_rglob_recurse_symlinks_false (__main__.PathSubclassTest.test_rglob_recurse_symlinks_false)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3164, in test_rglob_recurse_symlinks_false
    self.assertIsInstance(it, collections.abc.Iterator)
AssertionError: <map object> is not an instance of <class 'collections.abc.Iterator'>

======================================================================
FAIL: test_rglob_symlink_loop (__main__.PathSubclassTest.test_rglob_symlink_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3232, in test_rglob_symlink_loop
    self.assertEqual(given, {p / x for x in expect})
AssertionError: Items in the first set but not the second:
cls('/home/@test_N_tmpæ/brokenLink')
cls('/home/@test_N_tmpæ/brokenLinkLoop')
cls('/home/@test_N_tmpæ/dirA')
cls('/home/@test_N_tmpæ/dirB')
cls('/home/@test_N_tmpæ/dirC')
cls('/home/@test_N_tmpæ/dirE')
cls('/home/@test_N_tmpæ/fileA')
cls('/home/@test_N_tmpæ/linkA')
cls('/home/@test_N_tmpæ/linkB')
cls('/home/@test_N_tmpæ/dirA/linkC')
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirB/linkD')
cls('/home/@test_N_tmpæ/dirC/dirD')
cls('/home/@test_N_tmpæ/dirC/fileC')
cls('/home/@test_N_tmpæ/dirC/novel.txt')
cls('/home/@test_N_tmpæ/dirC/dirD/fileD')
Items in the second set but not the first:
cls('/home/@test_N_tmpæ/brokenLink')
cls('/home/@test_N_tmpæ/dirA')
cls('/home/@test_N_tmpæ/dirA/linkC')
cls('/home/@test_N_tmpæ/dirB')
cls('/home/@test_N_tmpæ/dirB/fileB')
cls('/home/@test_N_tmpæ/dirB/linkD')
cls('/home/@test_N_tmpæ/dirC')
cls('/home/@test_N_tmpæ/dirC/dirD')
cls('/home/@test_N_tmpæ/dirC/dirD/fileD')
cls('/home/@test_N_tmpæ/dirC/fileC')
cls('/home/@test_N_tmpæ/dirC/novel.txt')
cls('/home/@test_N_tmpæ/dirE')
cls('/home/@test_N_tmpæ/fileA')
cls('/home/@test_N_tmpæ/linkA')
cls('/home/@test_N_tmpæ/linkB')
cls('/home/@test_N_tmpæ/brokenLinkLoop')

======================================================================
FAIL: test_symlink_to (__main__.PathSubclassTest.test_symlink_to)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2612, in test_symlink_to
    self.assertEqual(link.stat(), target.stat())
AssertionError: os.st[27 chars]_ino=1682480274, st_dev=1, st_nlink=1, st_uid=[82 chars]5190) != os.st[27 chars]_ino=457006306, st_dev=1, st_nlink=1, st_uid=0[81 chars]5190)

======================================================================
FAIL: test_glob_dot (__main__.PathTest.test_glob_dot)
----------------------------------------------------------------------
AssertionError: Items in the first set but not the second:
PosixPath('dirD')
PosixPath('fileC')
PosixPath('novel.txt')
Items in the second set but not the first:
PosixPath('fileC')
PosixPath('novel.txt')
PosixPath('dirD')

======================================================================
FAIL: test_glob_dotdot (__main__.PathTest.test_glob_dotdot)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3054, in test_glob_dotdot
    self.assertEqual(set(p.glob("..")), { P(self.base, "..") })
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/..')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/..')

======================================================================
FAIL: test_glob_pathlike (__main__.PathTest.test_glob_pathlike)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3020, in test_glob_pathlike
    self.assertEqual(expect, set(p.glob(P(pattern))))
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')

======================================================================
FAIL: test_glob_recurse_symlinks_common (__main__.PathTest.test_glob_recurse_symlinks_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3097, in test_glob_recurse_symlinks_common
    _check(p, "dir*/file*", ["dirB/fileB", "dirC/fileC"])
  File "/tmp/test_pathlib.py", line 3093, in _check
    self.assertEqual(actual, { P(self.base, q) for q in expected })
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')

======================================================================
FAIL: test_is_char_device_true (__main__.PathTest.test_is_char_device_true)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2903, in test_is_char_device_true
    self.assertTrue(P.is_char_device())
AssertionError: False is not true

======================================================================
FAIL: test_is_mount (__main__.PathTest.test_is_mount)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2921, in test_is_mount
    self.assertTrue(R.is_mount())
AssertionError: False is not true

======================================================================
FAIL: test_iterdir_symlink (__main__.PathTest.test_iterdir_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2956, in test_iterdir_symlink
    self.assertEqual(paths, expected)
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD')

======================================================================
FAIL: test_mkdir_parent_mode_deep_hierarchy (__main__.PathTest.test_mkdir_parent_mode_deep_hierarchy)
----------------------------------------------------------------------
AssertionError: 493 != 448

======================================================================
FAIL: test_mkdir_parent_mode_same_as_mode (__main__.PathTest.test_mkdir_parent_mode_same_as_mode)
----------------------------------------------------------------------
AssertionError: 493 != 453

======================================================================
FAIL: test_mkdir_parents (__main__.PathTest.test_mkdir_parents)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2392, in test_mkdir_parents
    self.assertEqual(stat.S_IMODE(p.stat().st_mode), 0o7555 & mode)
AssertionError: 493 != 365

======================================================================
FAIL: test_mkdir_parents_umask (__main__.PathTest.test_mkdir_parents_umask)
----------------------------------------------------------------------
AssertionError: 493 != 509

======================================================================
FAIL: test_mkdir_with_parent_mode (__main__.PathTest.test_mkdir_with_parent_mode)
----------------------------------------------------------------------
AssertionError: 493 != 488

======================================================================
FAIL: test_rglob_pathlike (__main__.PathTest.test_rglob_pathlike)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3085, in test_rglob_pathlike
    self.assertEqual(expect, set(p.rglob(P(pattern))))
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')

======================================================================
FAIL: test_rglob_recurse_symlinks_common (__main__.PathTest.test_rglob_recurse_symlinks_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3133, in test_rglob_recurse_symlinks_common
    _check(p, "fileB", ["dirB/fileB", "dirA/linkC/fileB", "linkB/fileB",
  File "/tmp/test_pathlib.py", line 3130, in _check
    self.assertEqual(actual, { P(self.base, q) for q in expected })
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/linkD/fileB')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD/fileB')

======================================================================
FAIL: test_rglob_recurse_symlinks_false (__main__.PathTest.test_rglob_recurse_symlinks_false)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3164, in test_rglob_recurse_symlinks_false
    self.assertIsInstance(it, collections.abc.Iterator)
AssertionError: <map object> is not an instance of <class 'collections.abc.Iterator'>

======================================================================
FAIL: test_rglob_symlink_loop (__main__.PathTest.test_rglob_symlink_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3232, in test_rglob_symlink_loop
    self.assertEqual(given, {p / x for x in expect})
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/brokenLink')
PosixPath('/home/@test_N_tmpæ/brokenLinkLoop')
PosixPath('/home/@test_N_tmpæ/dirA')
PosixPath('/home/@test_N_tmpæ/dirB')
PosixPath('/home/@test_N_tmpæ/dirC')
PosixPath('/home/@test_N_tmpæ/dirE')
PosixPath('/home/@test_N_tmpæ/fileA')
PosixPath('/home/@test_N_tmpæ/linkA')
PosixPath('/home/@test_N_tmpæ/linkB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC')
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD')
PosixPath('/home/@test_N_tmpæ/dirC/dirD')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/novel.txt')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/brokenLink')
PosixPath('/home/@test_N_tmpæ/dirA')
PosixPath('/home/@test_N_tmpæ/dirA/linkC')
PosixPath('/home/@test_N_tmpæ/dirB')
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD')
PosixPath('/home/@test_N_tmpæ/dirC')
PosixPath('/home/@test_N_tmpæ/dirC/dirD')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/novel.txt')
PosixPath('/home/@test_N_tmpæ/dirE')
PosixPath('/home/@test_N_tmpæ/fileA')
PosixPath('/home/@test_N_tmpæ/linkA')
PosixPath('/home/@test_N_tmpæ/linkB')
PosixPath('/home/@test_N_tmpæ/brokenLinkLoop')

======================================================================
FAIL: test_symlink_to (__main__.PathTest.test_symlink_to)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2612, in test_symlink_to
    self.assertEqual(link.stat(), target.stat())
AssertionError: os.st[27 chars]_ino=1682480274, st_dev=1, st_nlink=1, st_uid=[82 chars]8434) != os.st[27 chars]_ino=457006306, st_dev=1, st_nlink=1, st_uid=0[81 chars]8434)

======================================================================
FAIL: test_glob_dot (__main__.PosixPathTest.test_glob_dot)
----------------------------------------------------------------------
AssertionError: Items in the first set but not the second:
PosixPath('dirD')
PosixPath('fileC')
PosixPath('novel.txt')
Items in the second set but not the first:
PosixPath('fileC')
PosixPath('novel.txt')
PosixPath('dirD')

======================================================================
FAIL: test_glob_dotdot (__main__.PosixPathTest.test_glob_dotdot)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3054, in test_glob_dotdot
    self.assertEqual(set(p.glob("..")), { P(self.base, "..") })
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/..')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/..')

======================================================================
FAIL: test_glob_pathlike (__main__.PosixPathTest.test_glob_pathlike)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3020, in test_glob_pathlike
    self.assertEqual(expect, set(p.glob(P(pattern))))
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')

======================================================================
FAIL: test_glob_recurse_symlinks_common (__main__.PosixPathTest.test_glob_recurse_symlinks_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3097, in test_glob_recurse_symlinks_common
    _check(p, "dir*/file*", ["dirB/fileB", "dirC/fileC"])
  File "/tmp/test_pathlib.py", line 3093, in _check
    self.assertEqual(actual, { P(self.base, q) for q in expected })
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')

======================================================================
FAIL: test_is_char_device_true (__main__.PosixPathTest.test_is_char_device_true)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2903, in test_is_char_device_true
    self.assertTrue(P.is_char_device())
AssertionError: False is not true

======================================================================
FAIL: test_is_mount (__main__.PosixPathTest.test_is_mount)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2921, in test_is_mount
    self.assertTrue(R.is_mount())
AssertionError: False is not true

======================================================================
FAIL: test_iterdir_symlink (__main__.PosixPathTest.test_iterdir_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2956, in test_iterdir_symlink
    self.assertEqual(paths, expected)
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD')

======================================================================
FAIL: test_mkdir_parent_mode_deep_hierarchy (__main__.PosixPathTest.test_mkdir_parent_mode_deep_hierarchy)
----------------------------------------------------------------------
AssertionError: 493 != 448

======================================================================
FAIL: test_mkdir_parent_mode_same_as_mode (__main__.PosixPathTest.test_mkdir_parent_mode_same_as_mode)
----------------------------------------------------------------------
AssertionError: 493 != 453

======================================================================
FAIL: test_mkdir_parents (__main__.PosixPathTest.test_mkdir_parents)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2392, in test_mkdir_parents
    self.assertEqual(stat.S_IMODE(p.stat().st_mode), 0o7555 & mode)
AssertionError: 493 != 365

======================================================================
FAIL: test_mkdir_parents_umask (__main__.PosixPathTest.test_mkdir_parents_umask)
----------------------------------------------------------------------
AssertionError: 493 != 509

======================================================================
FAIL: test_mkdir_with_parent_mode (__main__.PosixPathTest.test_mkdir_with_parent_mode)
----------------------------------------------------------------------
AssertionError: 493 != 488

======================================================================
FAIL: test_rglob_pathlike (__main__.PosixPathTest.test_rglob_pathlike)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3085, in test_rglob_pathlike
    self.assertEqual(expect, set(p.rglob(P(pattern))))
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')

======================================================================
FAIL: test_rglob_recurse_symlinks_common (__main__.PosixPathTest.test_rglob_recurse_symlinks_common)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3133, in test_rglob_recurse_symlinks_common
    _check(p, "fileB", ["dirB/fileB", "dirA/linkC/fileB", "linkB/fileB",
  File "/tmp/test_pathlib.py", line 3130, in _check
    self.assertEqual(actual, { P(self.base, q) for q in expected })
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/linkD/fileB')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/fileB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD/fileB')
PosixPath('/home/@test_N_tmpæ/linkB/linkD/fileB')

======================================================================
FAIL: test_rglob_recurse_symlinks_false (__main__.PosixPathTest.test_rglob_recurse_symlinks_false)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3164, in test_rglob_recurse_symlinks_false
    self.assertIsInstance(it, collections.abc.Iterator)
AssertionError: <map object> is not an instance of <class 'collections.abc.Iterator'>

======================================================================
FAIL: test_rglob_symlink_loop (__main__.PosixPathTest.test_rglob_symlink_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 3232, in test_rglob_symlink_loop
    self.assertEqual(given, {p / x for x in expect})
AssertionError: Items in the first set but not the second:
PosixPath('/home/@test_N_tmpæ/brokenLink')
PosixPath('/home/@test_N_tmpæ/brokenLinkLoop')
PosixPath('/home/@test_N_tmpæ/dirA')
PosixPath('/home/@test_N_tmpæ/dirB')
PosixPath('/home/@test_N_tmpæ/dirC')
PosixPath('/home/@test_N_tmpæ/dirE')
PosixPath('/home/@test_N_tmpæ/fileA')
PosixPath('/home/@test_N_tmpæ/linkA')
PosixPath('/home/@test_N_tmpæ/linkB')
PosixPath('/home/@test_N_tmpæ/dirA/linkC')
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD')
PosixPath('/home/@test_N_tmpæ/dirC/dirD')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/novel.txt')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')
Items in the second set but not the first:
PosixPath('/home/@test_N_tmpæ/brokenLink')
PosixPath('/home/@test_N_tmpæ/dirA')
PosixPath('/home/@test_N_tmpæ/dirA/linkC')
PosixPath('/home/@test_N_tmpæ/dirB')
PosixPath('/home/@test_N_tmpæ/dirB/fileB')
PosixPath('/home/@test_N_tmpæ/dirB/linkD')
PosixPath('/home/@test_N_tmpæ/dirC')
PosixPath('/home/@test_N_tmpæ/dirC/dirD')
PosixPath('/home/@test_N_tmpæ/dirC/dirD/fileD')
PosixPath('/home/@test_N_tmpæ/dirC/fileC')
PosixPath('/home/@test_N_tmpæ/dirC/novel.txt')
PosixPath('/home/@test_N_tmpæ/dirE')
PosixPath('/home/@test_N_tmpæ/fileA')
PosixPath('/home/@test_N_tmpæ/linkA')
PosixPath('/home/@test_N_tmpæ/linkB')
PosixPath('/home/@test_N_tmpæ/brokenLinkLoop')

======================================================================
FAIL: test_symlink_to (__main__.PosixPathTest.test_symlink_to)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_pathlib.py", line 2612, in test_symlink_to
    self.assertEqual(link.stat(), target.stat())
AssertionError: os.st[27 chars]_ino=1682480274, st_dev=1, st_nlink=1, st_uid=[82 chars]1753) != os.st[27 chars]_ino=457006306, st_dev=1, st_nlink=1, st_uid=0[81 chars]1753)

----------------------------------------------------------------------
Ran 1151 tests in Ns

FAILED (failures=51, errors=10, skipped=428)
