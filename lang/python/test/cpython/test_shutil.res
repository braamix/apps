......E.......EE.EE.ss....E...Es....s...sF.sF..s.ss.Fs.sFsssE.......s....s.............sss........s..s..F.....FF.....FFsss.s.....sss..ss..s..s....ssss......s.sssssss..sss...ssss......s.sssssss..sssssssssssssssssssssssssssssssssssssss
======================================================================
ERROR: test_make_archive_owner_group (__main__.TestArchives.test_make_archive_owner_group)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 2010, in test_make_archive_owner_group
    res = make_archive(base_name, 'zip', root_dir, base_dir, owner=owner,
  File "/pkg/store/python-0/lib/shutil.py", line 1261, in make_archive
    filename = func(base_name, base_dir, **kwargs)
  File "/pkg/store/python-0/lib/shutil.py", line 1114, in _make_zipfile
    zf.write(base_dir, arcname)
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 2560, in write
    zinfo = ZipInfo.from_file(filename, arcname,
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 660, in from_file
    zinfo = cls(arcname, date_time)
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 470, in __init__
    raise ValueError('ZIP does not support timestamps before 1980')
ValueError: ZIP does not support timestamps before 1980

======================================================================
ERROR: test_make_zipfile (__main__.TestArchives.test_make_zipfile)
----------------------------------------------------------------------
ValueError: ZIP does not support timestamps before 1980

======================================================================
ERROR: test_make_zipfile_in_curdir (__main__.TestArchives.test_make_zipfile_in_curdir)
----------------------------------------------------------------------
PermissionError: [Errno 13] Permission denied: 'test.zip'

======================================================================
ERROR: test_make_zipfile_with_explicit_curdir (__main__.TestArchives.test_make_zipfile_with_explicit_curdir)
----------------------------------------------------------------------
ValueError: ZIP does not support timestamps before 1980

======================================================================
ERROR: test_make_zipfile_without_rootdir (__main__.TestArchives.test_make_zipfile_without_rootdir)
----------------------------------------------------------------------
ValueError: ZIP does not support timestamps before 1980

======================================================================
ERROR: test_unpack_archive_zip (__main__.TestArchives.test_unpack_archive_zip)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 2271, in test_unpack_archive_zip
    self.check_unpack_archive('zip')
  File "/tmp/test_shutil.py", line 2217, in check_unpack_archive
    self.check_unpack_archive_with_converter(
  File "/tmp/test_shutil.py", line 2227, in check_unpack_archive_with_converter
    filename = make_archive(base_name, format, root_dir, base_dir)
  File "/pkg/store/python-0/lib/shutil.py", line 1261, in make_archive
    filename = func(base_name, base_dir, **kwargs)
  File "/pkg/store/python-0/lib/shutil.py", line 1114, in _make_zipfile
    zf.write(base_dir, arcname)
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 2560, in write
    zinfo = ZipInfo.from_file(filename, arcname,
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 660, in from_file
    zinfo = cls(arcname, date_time)
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 470, in __init__
    raise ValueError('ZIP does not support timestamps before 1980')
ValueError: ZIP does not support timestamps before 1980

======================================================================
ERROR: test_unzip_zipfile (__main__.TestArchives.test_unzip_zipfile)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 1972, in test_unzip_zipfile
    archive = make_archive(base_name, 'zip', root_dir, base_dir)
  File "/pkg/store/python-0/lib/shutil.py", line 1261, in make_archive
    filename = func(base_name, base_dir, **kwargs)
  File "/pkg/store/python-0/lib/shutil.py", line 1114, in _make_zipfile
    zf.write(base_dir, arcname)
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 2560, in write
    zinfo = ZipInfo.from_file(filename, arcname,
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 660, in from_file
    zinfo = cls(arcname, date_time)
  File "/pkg/store/python-0/lib/zipfile/__init__.py", line 470, in __init__
    raise ValueError('ZIP does not support timestamps before 1980')
ValueError: ZIP does not support timestamps before 1980

======================================================================
ERROR: test_dont_copy_file_onto_symlink_to_itself (__main__.TestCopy.test_dont_copy_file_onto_symlink_to_itself)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 1529, in test_dont_copy_file_onto_symlink_to_itself
    os.mkdir(TESTFN)
FileExistsError: [Errno 17] File exists: '@test_N_tmpæ'

======================================================================
FAIL: test_copyfile_character_device (__main__.TestCopy.test_copyfile_character_device)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 1621, in test_copyfile_character_device
    self.assertRaisesRegex(shutil.SpecialFileError, 'is a character device',
AssertionError: SpecialFileError not raised by copyfile

======================================================================
FAIL: test_copyfile_nonexistent_dir (__main__.TestCopy.test_copyfile_nonexistent_dir)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 1674, in test_copyfile_nonexistent_dir
    self.assertRaises(FileNotFoundError, shutil.copyfile, src_file, dst)
AssertionError: FileNotFoundError not raised by copyfile

======================================================================
FAIL: test_copymode_follow_symlinks (__main__.TestCopy.test_copymode_follow_symlinks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 1122, in test_copymode_follow_symlinks
    self.assertNotEqual(os.stat(src).st_mode, os.stat(dst).st_mode)
AssertionError: 33188 == 33188

======================================================================
FAIL: test_copystat_symlinks (__main__.TestCopy.test_copystat_symlinks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 1224, in test_copystat_symlinks
    self.assertTrue(abs(os.stat(src).st_mtime - os.stat(dst).st_mtime) <
AssertionError: False is not true

======================================================================
FAIL: test_move_dir_symlink (__main__.TestMove.test_move_dir_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 68, in wrap
    return func(*args, **kwargs)
  File "/tmp/test_shutil.py", line 3063, in test_move_dir_symlink
    self.assertTrue(os.path.samefile(src, dst_link))
AssertionError: False is not true

======================================================================
FAIL: test_move_file_symlink (__main__.TestMove.test_move_file_symlink)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 68, in wrap
    return func(*args, **kwargs)
  File "/tmp/test_shutil.py", line 3029, in test_move_file_symlink
    self.assertTrue(os.path.samefile(self.src_file, self.dst_file))
AssertionError: False is not true

======================================================================
FAIL: test_move_file_symlink_to_dir (__main__.TestMove.test_move_file_symlink_to_dir)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 68, in wrap
    return func(*args, **kwargs)
  File "/tmp/test_shutil.py", line 3040, in test_move_file_symlink_to_dir
    self.assertTrue(os.path.samefile(self.src_file, final_link))
AssertionError: False is not true

======================================================================
FAIL: test_move_symlink_to_dir_into_dir (__main__.TestMove.test_move_symlink_to_dir_into_dir)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 3130, in test_move_symlink_to_dir_into_dir
    self._test_move_symlink_to_dir_into_dir(self.dst_dir)
  File "/tmp/test_shutil.py", line 68, in wrap
    return func(*args, **kwargs)
  File "/tmp/test_shutil.py", line 3117, in _test_move_symlink_to_dir_into_dir
    self.assertTrue(os.path.samefile(self.dst_dir, dst_link))
AssertionError: False is not true

======================================================================
FAIL: test_move_symlink_to_dir_into_symlink_to_dir (__main__.TestMove.test_move_symlink_to_dir_into_symlink_to_dir)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_shutil.py", line 3136, in test_move_symlink_to_dir_into_symlink_to_dir
    self._test_move_symlink_to_dir_into_dir(dst)
  File "/tmp/test_shutil.py", line 68, in wrap
    return func(*args, **kwargs)
  File "/tmp/test_shutil.py", line 3117, in _test_move_symlink_to_dir_into_dir
    self.assertTrue(os.path.samefile(self.dst_dir, dst_link))
AssertionError: False is not true

----------------------------------------------------------------------
Ran 233 tests in Ns

FAILED (failures=9, errors=8, skipped=98)
