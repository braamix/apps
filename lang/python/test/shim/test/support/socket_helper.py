"""The names test/cpython/'s tests take from test.support.socket_helper.

There are no sockets here, so everything that would open one raises where
upstream's would connect. The three host names are constants and are real.
"""

import unittest

HOST = "localhost"
HOSTv4 = "127.0.0.1"
HOSTv6 = "::1"


def find_unused_port(family=None, socktype=None):
    raise unittest.SkipTest("no sockets")


def bind_port(sock, host=HOST):
    raise unittest.SkipTest("no sockets")


def bind_unix_socket(sock, addr):
    raise unittest.SkipTest("no sockets")


def get_socket_conn_refused_errs():
    import errno
    return [errno.ECONNREFUSED]


def skip_unless_bind_unix_socket(test):
    return unittest.skip("no sockets")(test)


def skip_if_tcp_blackhole(test):
    return test


def tcp_blackhole():
    return False


def create_unix_domain_name():
    raise unittest.SkipTest("no sockets")
