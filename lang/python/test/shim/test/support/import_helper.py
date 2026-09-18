"""The part of test.support.import_helper the tests here reach.

Upstream's imports importlib, which is not here yet; __import__ and
sys.modules do the same work for a module found on sys.path.
"""

import sys
import unittest


def _import(name):
    __import__(name)
    return sys.modules[name]


def import_module(name, deprecated=False, *, required_on=()):
    """The module, or SkipTest when it cannot be imported."""
    try:
        return _import(name)
    except ImportError as msg:
        if sys.platform.startswith(tuple(required_on)):
            raise
        raise unittest.SkipTest(str(msg))


def _save_and_remove_modules(names):
    orig_modules = {}
    prefixes = tuple(name + '.' for name in names)
    for modname in list(sys.modules):
        if modname in names or modname.startswith(prefixes):
            orig_modules[modname] = sys.modules.pop(modname)
    return orig_modules


def import_fresh_module(name, fresh=(), blocked=(), *,
                        deprecated=False, usefrozen=False):
    """A fresh copy of the module, with `fresh` imported again and `blocked`
    refused. None when one of `fresh` cannot be imported."""
    fresh = list(fresh)
    blocked = list(blocked)
    names = {name, *fresh, *blocked}
    orig_modules = _save_and_remove_modules(names)
    for modname in blocked:
        sys.modules[modname] = None
    try:
        try:
            for modname in fresh:
                __import__(modname)
        except ImportError:
            return None
        return _import(name)
    finally:
        _save_and_remove_modules(names)
        sys.modules.update(orig_modules)


def ensure_lazy_imports(imported_module, modules_to_block, *, additional_code=None):
    """Test that when imported_module is imported, none of the modules in
    modules_to_block are imported as a side effect."""
    import textwrap

    modules_to_block = frozenset(modules_to_block)
    script = textwrap.dedent(
        f"""
        import sys
        modules_to_block = {modules_to_block}
        if unexpected := modules_to_block & sys.modules.keys():
            startup = ", ".join(unexpected)
            raise AssertionError(f'unexpectedly imported at startup: {{startup}}')

        import {imported_module}
        if unexpected := modules_to_block & sys.modules.keys():
            after = ", ".join(unexpected)
            raise AssertionError(f'unexpectedly imported after importing {imported_module}: {{after}}')
        """
    )
    if additional_code:
        script += additional_code
        script += textwrap.dedent(
            f"""
            if unexpected := modules_to_block & sys.modules.keys():
                after = ", ".join(unexpected)
                raise AssertionError(f'unexpectedly imported after additional code: {{after}}')
            """
        )

    from .script_helper import assert_python_ok
    assert_python_ok("-S", "-c", script)


def make_legacy_pyc(source, allow_compile=False):
    """There are no .pyc files here, so a test of one skips."""
    raise unittest.SkipTest("no .pyc files")
