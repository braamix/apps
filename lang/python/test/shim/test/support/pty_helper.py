"""The names test/cpython/'s tests take from test.support.pty_helper.

There is no pty here and no way to give a program a terminal of its own, so
`run_pty` skips. `FakeInput` is upstream's, byte for byte: it is a stand-in
for stdin and needs nothing of the system.
"""

import unittest


def run_pty(script, input=b"dummy input\r", env=None):
    raise unittest.SkipTest("no pty")


class FakeInput:
    """
    A fake input stream for pdb's interactive debugger.  Whenever a
    line is read, print it (to simulate the user typing it), and then
    return it.  The set of lines to return is specified in the
    constructor; they should not have trailing newlines.
    """
    def __init__(self, lines):
        self.lines = lines

    def readline(self):
        line = self.lines.pop(0)
        print(line)
        return line + '\n'
