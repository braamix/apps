"""test.support.i18n_helper: the snapshots are made by running pygettext in a
second interpreter, and there is no subprocess, so the test skips."""

import unittest


@unittest.skip("no subprocess")
class TestTranslationsBase(unittest.TestCase):
    pass


def update_translation_snapshots(module):
    raise unittest.SkipTest("no subprocess")
