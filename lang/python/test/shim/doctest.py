"""doctest: not here yet (TODO.md task 31, which needs pdb and so socket).

CPython's tests import it only to add a module's doctests to their suite, so
that suite is empty here and the rest of the test runs.
"""

import unittest

# The option flags, as doctest numbers them.
DONT_ACCEPT_TRUE_FOR_1 = 1 << 0
DONT_ACCEPT_BLANKLINE = 1 << 1
NORMALIZE_WHITESPACE = 1 << 2
ELLIPSIS = 1 << 3
SKIP = 1 << 4
IGNORE_EXCEPTION_DETAIL = 1 << 5


def DocTestSuite(module=None, *args, **kwargs):
    return unittest.TestSuite()


def DocFileSuite(*paths, **kwargs):
    return unittest.TestSuite()
