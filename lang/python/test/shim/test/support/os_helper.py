"""The names CPython's tests take from test.support.os_helper.

Upstream's probes what the platform can do; this says what Braam's store
does: symlinks yes, hard links, extended attributes, modes and owners no.
The file names are upstream's, so a test that prints one prints the same.
"""

import collections.abc
import contextlib
import os
import sys
import unittest

TESTFN_ASCII = "@test_{}_tmp".format(os.getpid())
TESTFN_UNICODE = TESTFN_ASCII + "-\xe0\xf2ɘŁğ"
# UTF-8 cannot say the byte 0xff, which surrogateescape turns into U+DCFF.
TESTFN_UNENCODABLE = TESTFN_ASCII + "-\udcff"
TESTFN_UNDECODABLE = os.fsencode(TESTFN_ASCII) + b"\xe7w\xf0"
FS_NONASCII = "æ"
TESTFN_NONASCII = TESTFN_ASCII + FS_NONASCII
TESTFN = TESTFN_NONASCII

SAVEDCWD = os.getcwd()


def make_bad_fd():
    """A descriptor that was open once and is not now."""
    file = open(TESTFN, "wb")
    try:
        return file.fileno()
    finally:
        file.close()
        unlink(TESTFN)


def can_symlink():
    return True


def skip_unless_symlink(test):
    return test


def can_hardlink():
    return False


def skip_unless_hardlink(test):
    return unittest.skip("requires hardlink support")(test)


def can_xattr():
    return False


def skip_unless_xattr(test):
    return unittest.skip("no non-broken extended attribute support")(test)


def can_chmod():
    return False


def skip_unless_working_chmod(test):
    return unittest.skip("requires working os.chmod()")(test)


def can_dac_override():
    return False


def skip_if_dac_override(test):
    return test


def skip_unless_dac_override(test):
    return unittest.skip("incapable of DAC override")(test)


@contextlib.contextmanager
def save_mode(path, *, quiet=False):
    yield


def unlink(filename):
    try:
        os.unlink(filename)
    except (FileNotFoundError, NotADirectoryError):
        pass


def rmdir(dirname):
    try:
        os.rmdir(dirname)
    except FileNotFoundError:
        pass


def rmtree(path):
    import shutil
    try:
        shutil.rmtree(path)
    except FileNotFoundError:
        pass
    except NotADirectoryError:
        unlink(path)


@contextlib.contextmanager
def temp_dir(path=None, quiet=False):
    """A directory that is there for the block and gone after it."""
    import tempfile
    created = False
    if path is None:
        path = os.path.realpath(tempfile.mkdtemp())
        created = True
    else:
        try:
            os.mkdir(path)
            created = True
        except OSError:
            if not quiet:
                raise
    try:
        yield path
    finally:
        if created:
            rmtree(path)


@contextlib.contextmanager
def change_cwd(path, quiet=False):
    saved = os.getcwd()
    try:
        os.chdir(os.path.realpath(path))
    except OSError:
        if not quiet:
            raise
    try:
        yield os.getcwd()
    finally:
        os.chdir(saved)


@contextlib.contextmanager
def temp_cwd(name="tempcwd", quiet=False):
    with temp_dir(path=name, quiet=quiet) as temp_path:
        with change_cwd(temp_path, quiet=quiet) as cwd_dir:
            yield cwd_dir


def create_empty_file(filename):
    fd = os.open(filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC)
    os.close(fd)


@contextlib.contextmanager
def open_dir_fd(path):
    raise unittest.SkipTest("a directory cannot be opened here")
    yield


def fs_is_case_insensitive(directory):
    return False


class FakePath:
    """The path protocol and nothing else."""

    def __init__(self, path):
        self.path = path

    def __repr__(self):
        return f"<FakePath {self.path!r}>"

    def __fspath__(self):
        if (isinstance(self.path, BaseException) or
                isinstance(self.path, type) and issubclass(self.path, BaseException)):
            raise self.path
        return self.path


def fd_count():
    """How many descriptors are open: those that dup() will copy."""
    count = 0
    for fd in range(64):
        try:
            fd2 = os.dup(fd)
        except OSError:
            continue
        os.close(fd2)
        count += 1
    return count


@contextlib.contextmanager
def temp_umask(umask):
    old = os.umask(umask)
    try:
        yield
    finally:
        os.umask(old)


class EnvironmentVarGuard(collections.abc.MutableMapping):
    """os.environ, put back as it was when the block ends."""

    def __init__(self):
        self._environ = os.environ
        self._changed = {}

    def __getitem__(self, envvar):
        return self._environ[envvar]

    def __setitem__(self, envvar, value):
        if envvar not in self._changed:
            self._changed[envvar] = self._environ.get(envvar)
        self._environ[envvar] = value

    def __delitem__(self, envvar):
        if envvar not in self._changed:
            self._changed[envvar] = self._environ.get(envvar)
        if envvar in self._environ:
            del self._environ[envvar]

    def keys(self):
        return self._environ.keys()

    def __iter__(self):
        return iter(self._environ)

    def __len__(self):
        return len(self._environ)

    def set(self, envvar, value):
        self[envvar] = value

    def unset(self, envvar, /, *envvars):
        for ev in (envvar, *envvars):
            del self[ev]

    def copy(self):
        return dict(self)

    def __enter__(self):
        return self

    def __exit__(self, *ignore_exc):
        for k, v in self._changed.items():
            if v is None:
                if k in self._environ:
                    del self._environ[k]
            else:
                self._environ[k] = v
        os.environ = self._environ


def without_source_date_epoch(fxn):
    return fxn


def with_source_date_epoch(fxn=None, *, epoch=123456789):
    if fxn is None:
        return lambda f: f
    return fxn
