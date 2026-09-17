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
    """Upstream runs a second interpreter for this; there is none."""
    raise unittest.SkipTest("no subprocess")
