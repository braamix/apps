# The real unittest, over inspect and logging.
import io
import re
import unittest


class T(unittest.TestCase):
    def setUp(self):
        self.made = []

    def tearDown(self):
        self.made.append("torn")

    def test_ok(self):
        self.assertEqual(1 + 1, 2)
        self.assertIn(3, [1, 2, 3])
        self.assertAlmostEqual(0.1 + 0.2, 0.3)
        self.assertCountEqual([1, 2, 2], [2, 1, 2])
        self.assertRegex("abc", "b")
        self.assertIsInstance(1, int)

    def test_raises(self):
        with self.assertRaises(ValueError):
            int("x")
        with self.assertRaisesRegex(TypeError, "unsupported"):
            1 + "a"

    def test_sub(self):
        for i in range(3):
            with self.subTest(i=i):
                self.assertLess(i, 3)

    @unittest.skip("because")
    def test_skipped(self):
        self.fail()

    @unittest.expectedFailure
    def test_expected(self):
        self.assertEqual(1, 2)

    def test_fails(self):
        self.assertEqual(1, 2)

    def test_errors(self):
        raise RuntimeError("boom")


class Other(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.seen = True

    def test_class_setup(self):
        self.assertTrue(self.seen)


stream = io.StringIO()
runner = unittest.TextTestRunner(stream=stream, verbosity=2)
suite = unittest.TestLoader().loadTestsFromModule(__import__("__main__"))
result = runner.run(suite)

print("ran", result.testsRun)
print("failures", len(result.failures))
print("errors", len(result.errors))
print("skipped", result.skipped)
print("expected", len(result.expectedFailures))
print("wasSuccessful", result.wasSuccessful())

# The report, with what moves between runs taken out: the elapsed time, the
# path of this file and the caret line a traceback draws.
text = stream.getvalue()
text = re.sub(r"in [0-9.]+s", "in Ns", text)
text = re.sub(r'File "[^"]*"', 'File "F"', text)
text = "\n".join(ln for ln in text.split("\n") if not ln.strip().startswith("~"))
print(text)

# unittest.mock over pkgutil and dataclasses.
from unittest.mock import MagicMock, Mock, patch, call, sentinel, ANY, create_autospec

m = MagicMock(return_value=3)
print(m(1, 2), m.call_args, m.called, m.call_count)
m.assert_called_once_with(1, 2)
print(m.child.grandchild(4).__class__.__name__)
print(len(m.mock_calls) > 0, call(1, 2) in m.call_args_list)

s = Mock(spec=["a", "b"])
s.a()
print(s.a.called, hasattr(s, "c"))


class Target:
    def method(self, x):
        return x


t = Target()
with patch.object(Target, "method", return_value=99) as p:
    print(t.method(1), p.called)
print(t.method(1))

auto = create_autospec(Target)
auto.method(5)
auto.method.assert_called_with(5)
print(sentinel.thing is sentinel.thing, ANY == 12345)
print(Mock(name="n")._mock_name)
