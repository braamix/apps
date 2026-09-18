"""test.support.script_helper: a second interpreter, as upstream's runs one.

Upstream reads the child's stdout and stderr through two pipes in one
communicate(), which waits in selectors on a readiness call Braam has not got.
Here stdout is the one pipe and stderr goes to a temporary file read back
afterwards; stdin is /dev/null, since nothing is written to it. The names and
what they return are upstream's.
"""

import collections
import os
import subprocess
import sys
import tempfile

from test import support


def interpreter_requires_environment():
    # Nothing here reads PYTHONHOME, so -E and -I cannot stop it starting.
    return False


class _PythonRunResult(collections.namedtuple("_PythonRunResult", ("rc", "out", "err"))):
    """Helper for reporting Python subprocess run results"""

    def fail(self, cmd_line):
        """Provide helpful details about failed subcommand runs"""
        maxlen = 300 * 100
        out, err = self.out, self.err
        if len(out) > maxlen:
            out = b"(... truncated stdout ...)" + out[-maxlen:]
        if len(err) > maxlen:
            err = b"(... truncated stderr ...)" + err[-maxlen:]
        out = out.decode("utf8", "replace").rstrip()
        err = err.decode("utf8", "replace").rstrip()
        raise AssertionError(
            f"Process return code is {self.rc}\n"
            f"command line: {cmd_line!r}\n"
            f"\n"
            f"stdout:\n"
            f"---\n"
            f"{out}\n"
            f"---\n"
            f"\n"
            f"stderr:\n"
            f"---\n"
            f"{err}\n"
            f"---"
        )


def run_python_until_end(*args, **env_vars):
    """Used to implement assert_python_*."""
    run_using_command = env_vars.pop("__run_using_command", None)
    cwd = env_vars.pop("__cwd", None)
    if "__isolated" in env_vars:
        isolated = env_vars.pop("__isolated")
    else:
        isolated = not env_vars
    cmd_line = [sys.executable, "-X", "faulthandler"]
    if run_using_command:
        cmd_line = run_using_command + cmd_line
    if isolated:
        cmd_line.append("-I")
    elif not env_vars:
        cmd_line.append("-E")

    if env_vars.pop("__cleanenv", None):
        env = {}
    else:
        env = os.environ.copy()
    if "TERM" not in env_vars:
        env["TERM"] = ""
    env.update(env_vars)
    cmd_line.extend(args)
    with tempfile.TemporaryFile() as errfile:
        proc = subprocess.Popen(cmd_line, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=errfile,
                                env=env, cwd=cwd)
        with proc:
            try:
                out, _ = proc.communicate()
            finally:
                proc.kill()
                subprocess._cleanup()
        errfile.seek(0)
        err = errfile.read()
    rc = proc.returncode
    return _PythonRunResult(rc, out, err), cmd_line


def _assert_python(expected_success, /, *args, **env_vars):
    res, cmd_line = run_python_until_end(*args, **env_vars)
    if (res.rc and expected_success) or (not res.rc and not expected_success):
        res.fail(cmd_line)
    return res


def assert_python_ok(*args, **env_vars):
    return _assert_python(True, *args, **env_vars)


def assert_python_failure(*args, **env_vars):
    return _assert_python(False, *args, **env_vars)


def spawn_python(*args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kw):
    """Run a Python subprocess with the given arguments. Returns a Popen."""
    cmd_line = [sys.executable, "-E"]
    cmd_line.extend(args)
    env = kw.setdefault("env", dict(os.environ))
    env["TERM"] = "vt100"
    return subprocess.Popen(cmd_line, stdin=subprocess.PIPE, stdout=stdout, stderr=stderr, **kw)


def kill_python(p):
    """Run the given Popen process until completion and return stdout."""
    p.stdin.close()
    data = p.stdout.read()
    p.stdout.close()
    p.wait()
    subprocess._cleanup()
    return data


def make_script(script_dir, script_basename, source, omit_suffix=False):
    script_filename = script_basename
    if not omit_suffix:
        script_filename += os.extsep + "py"
    script_name = os.path.join(script_dir, script_filename)
    if isinstance(source, str):
        with open(script_name, "w", encoding="utf-8") as script_file:
            script_file.write(source)
    else:
        with open(script_name, "wb") as script_file:
            script_file.write(source)
    # Upstream calls importlib.invalidate_caches() here, which imports
    # importlib.metadata, and that is not shipped.
    return script_name


def make_pkg(pkg_dir, init_source=""):
    os.mkdir(pkg_dir)
    make_script(pkg_dir, "__init__", init_source)


# A zip on sys.path is not imported from here.
def _no_zipimport(*args, **kwargs):
    import unittest

    raise unittest.SkipTest("no zipimport")


make_zip_script = _no_zipimport
make_zip_pkg = _no_zipimport


def run_test_script(script):
    assert_python_ok("-u", script, "-v")
