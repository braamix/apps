# The package CPython's own tests live in. Its tests reach `test.support`
# through it, so it exists and holds nothing -- except a way for a case to
# name itself.
#
# A case is planted as a file and run as `__main__`, and a few of them end by
# asking for themselves by their dotted name: `from test import test_pdb`,
# `unittest.main(module='test.test_doctest.test_doctest')`. Here that name
# means the module already running, which is what upstream's layout gives it.

import os
import sys


def __getattr__(name):
    main = sys.modules.get("__main__")
    path = getattr(main, "__file__", "") if main is not None else ""
    if path and os.path.basename(path) == name + ".py":
        sys.modules[__name__ + "." + name] = main
        return main
    raise AttributeError(f"cannot import name {name!r} from 'test'")
