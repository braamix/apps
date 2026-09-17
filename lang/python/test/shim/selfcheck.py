"""The shims' own test: the loader, the assertions, and every outcome.

Not one of CPython's, and never in cpython.txt. It is what says the mechanism
is sound on a day when no upstream test runs at all, and it is written to
produce one of each result -- ok, fail, error, skip and an expected failure --
so the harness's reading of the summary is exercised too.
"""

import unittest
from test import support


class Assertions(unittest.TestCase):
    def test_equality(self):
        self.assertEqual(1, 1)
        self.assertNotEqual(1, 2)
        self.assertEqual([1, 2], [1, 2])
        self.assertEqual({"a": 1}, {"a": 1})

    def test_truth(self):
        self.assertTrue(1)
        self.assertTrue([0])
        self.assertFalse(0)
        self.assertFalse([])

    def test_identity(self):
        self.assertIs(None, None)
        self.assertIsNot([], [])
        self.assertIsNone(None)
        self.assertIsNotNone(0)

    def test_membership(self):
        self.assertIn(1, [1, 2])
        self.assertNotIn(3, [1, 2])
        self.assertIn("ell", "hello")

    def test_types(self):
        self.assertIsInstance(1, int)
        self.assertIsInstance(1, (str, int))
        self.assertNotIsInstance(1, str)
        self.assertHasAttr([], "append")
        self.assertNotHasAttr([], "nope")

    def test_ordering(self):
        self.assertGreater(2, 1)
        self.assertGreaterEqual(1, 1)
        self.assertLess(1, 2)
        self.assertLessEqual(1, 1)

    def test_almost(self):
        self.assertAlmostEqual(1.0, 1.0 + 1e-9)
        self.assertNotAlmostEqual(1.0, 1.1)
        self.assertAlmostEqual(1.0, 1.2, delta=0.5)

    def test_sequences(self):
        self.assertSequenceEqual([1, 2], [1, 2])
        self.assertListEqual([1], [1])
        self.assertTupleEqual((1,), (1,))
        self.assertDictEqual({"a": 1}, {"a": 1})
        self.assertSetEqual({1, 2}, {2, 1})
        self.assertCountEqual([2, 1], [1, 2])
        self.assertMultiLineEqual("a\nb", "a\nb")


class Raising(unittest.TestCase):
    def test_context_manager(self):
        with self.assertRaises(ValueError):
            raise ValueError("boom")

    def test_exception_is_kept(self):
        with self.assertRaises(ValueError) as cm:
            raise ValueError("boom")
        self.assertEqual(str(cm.exception), "boom")

    def test_tuple_of_types(self):
        with self.assertRaises((TypeError, ValueError)):
            raise TypeError("t")

    def test_call_form(self):
        def explode(a, b):
            raise KeyError(a + b)
        self.assertRaises(KeyError, explode, 1, 2)
        self.assertRaisesRegex(KeyError, "3", explode, 1, 2)

    def test_call_form_lets_the_wrong_one_through(self):
        def explode():
            raise KeyError("k")
        try:
            self.assertRaises(ValueError, explode)
        except KeyError:
            return
        self.fail("an unexpected exception should have propagated")

    def test_call_form_that_raises_nothing_is_a_failure(self):
        try:
            self.assertRaises(ValueError, lambda: None)
        except AssertionError:
            return
        self.fail("a missing exception should have failed the test")

    def test_not_raised_is_a_failure(self):
        try:
            with self.assertRaises(ValueError):
                pass
        except AssertionError:
            return
        self.fail("a missing exception should have failed the test")

    def test_wrong_type_passes_through(self):
        try:
            with self.assertRaises(ValueError):
                raise KeyError("k")
        except KeyError:
            return
        self.fail("an unexpected exception should have propagated")

    def test_regex_plain(self):
        with self.assertRaisesRegex(ValueError, "not defined"):
            raise ValueError("name 'a' is not defined")

    def test_regex_anchored(self):
        with self.assertRaisesRegex(ValueError, "^exactly this$"):
            raise ValueError("exactly this")

    def test_regex_escape(self):
        with self.assertRaisesRegex(ValueError, r"takes 2 \(not 3\)"):
            raise ValueError("f() takes 2 (not 3)")

    def test_regex_that_does_not_match(self):
        try:
            with self.assertRaisesRegex(ValueError, "elsewhere"):
                raise ValueError("here")
        except AssertionError:
            return
        self.fail("a pattern that does not match should have failed the test")

    def test_regex_is_re(self):
        # The pattern is re's: a metacharacter means what it means there.
        with self.assertRaisesRegex(ValueError, "a.*b"):
            raise ValueError("axxb")
        try:
            with self.assertRaisesRegex(ValueError, r"^a\d$"):
                raise ValueError("ab")
        except AssertionError:
            return
        self.fail("a pattern that does not match should have failed the test")


class SubTests(unittest.TestCase):
    def test_all_good(self):
        for i in [1, 2, 3]:
            with self.subTest(i=i):
                self.assertTrue(i > 0)

    def test_skip_inside_is_not_a_failure(self):
        with self.subTest(part="one"):
            raise unittest.SkipTest("nothing here")


class Fixtures(unittest.TestCase):
    order = []

    def setUpClass():
        Fixtures.order.append("setUpClass")
    setUpClass = staticmethod(setUpClass)

    def setUp(self):
        Fixtures.order.append("setUp")

    def tearDown(self):
        Fixtures.order.append("tearDown")

    def test_a(self):
        self.assertEqual(Fixtures.order[0], "setUpClass")
        self.assertEqual(Fixtures.order[-1], "setUp")

    def test_b(self):
        self.assertIn("tearDown", Fixtures.order)


class Inherited(Assertions):
    """Every method of the base runs again here, plus this one."""

    def test_own(self):
        self.assertTrue(True)


# What a decorated test actually did, so a marker that was never unwrapped
# cannot look like a pass.
RAN = []


class Outcomes(unittest.TestCase):
    """One of each, so the summary line has every column exercised."""

    def test_this_one_fails(self):
        self.assertEqual(1, 2)

    def test_this_one_errors(self):
        raise KeyError("not an assertion")

    def test_this_one_skips(self):
        self.skipTest("deliberate")

    def test_subtest_failure_is_reported(self):
        for i in [1, 2]:
            with self.subTest(i=i):
                self.assertEqual(i, 1)

    def _expected(self):
        RAN.append("expected")
        self.assertEqual(1, 2)
    test_this_one_is_expected_to_fail = unittest.expectedFailure(_expected)

    test_this_one_is_skipped_by_decorator = unittest.skipIf(
        True, "skipIf said so")(lambda self: self.fail("should not run"))

    def _not_skipped(self):
        RAN.append("not skipped")
    test_this_one_is_not_skipped = unittest.skipUnless(
        True, "skipUnless said so")(_not_skipped)


@unittest.skipIf(True, "the whole class")
class SkippedWhole(unittest.TestCase):
    """A class decorated whole still appears, with every test skipped."""

    def test_one(self):
        self.fail("should not run")

    def test_two(self):
        self.fail("should not run")


class Wrapped(unittest.TestCase):
    """Named to sort last: the loader takes classes in name order, so what
    Outcomes did is readable by the time this runs."""

    def test_the_expected_failure_really_ran(self):
        self.assertIn("expected", RAN)

    def test_the_unskipped_one_really_ran(self):
        self.assertIn("not skipped", RAN)


class Support(unittest.TestCase):
    def test_get_attribute(self):
        found = []
        support.get_attribute(found, "append")(1)
        self.assertEqual(found, [1])
        with self.assertRaises(unittest.SkipTest):
            support.get_attribute(found, "nope")

    def test_never_and_always_equal(self):
        self.assertNotEqual(support.NEVER_EQ, support.NEVER_EQ)
        self.assertEqual(support.ALWAYS_EQ, 17)

    test_docstrings_are_skipped = support.requires_docstrings(
        lambda self: self.fail("there are no docstrings yet"))

    test_refcounts_are_skipped = support.refcount_test(
        lambda self: self.fail("there is no refcount"))


if __name__ == "__main__":
    unittest.main()
