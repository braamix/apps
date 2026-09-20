"""Enough of upstream's package for test_doctest to name itself.

The case ends with `unittest.main(module='test.test_doctest.test_doctest')`,
and a case here is planted as a file and run as `__main__`. This makes that
dotted name mean the module already running, which is what upstream's package
layout would have given it.
"""

import sys

test_doctest = sys.modules["__main__"]
sys.modules[__name__ + ".test_doctest"] = test_doctest
