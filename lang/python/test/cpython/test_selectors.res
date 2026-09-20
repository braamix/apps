EE..EEEEEEssEEEEEEEssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssEE..EEEEEEssEEEEEEEEE..EEEsEEssEEEEEEE
======================================================================
ERROR: test_close (__main__.DefaultSelectorTestCase.test_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 215, in test_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_context_manager (__main__.DefaultSelectorTestCase.test_context_manager)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 320, in test_context_manager
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_get_key (__main__.DefaultSelectorTestCase.test_get_key)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 232, in test_get_key
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_get_map (__main__.DefaultSelectorTestCase.test_get_map)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 244, in test_get_map
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_modify (__main__.DefaultSelectorTestCase.test_modify)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 149, in test_modify
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_modify_unregister (__main__.DefaultSelectorTestCase.test_modify_unregister)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 203, in test_modify_unregister
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_register (__main__.DefaultSelectorTestCase.test_register)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 67, in test_register
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_select (__main__.DefaultSelectorTestCase.test_select)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 273, in test_select
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_select_read_write (__main__.DefaultSelectorTestCase.test_select_read_write)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 293, in test_select_read_write
    sock1, sock2 = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_selector (__main__.DefaultSelectorTestCase.test_selector)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 351, in test_selector
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_timeout (__main__.DefaultSelectorTestCase.test_timeout)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 404, in test_timeout
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister (__main__.DefaultSelectorTestCase.test_unregister)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 93, in test_unregister
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_fd_close (__main__.DefaultSelectorTestCase.test_unregister_after_fd_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 107, in test_unregister_after_fd_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_fd_close_and_reuse (__main__.DefaultSelectorTestCase.test_unregister_after_fd_close_and_reuse)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 120, in test_unregister_after_fd_close_and_reuse
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_socket_close (__main__.DefaultSelectorTestCase.test_unregister_after_socket_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 137, in test_unregister_after_socket_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_close (__main__.PollSelectorTestCase.test_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 215, in test_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_context_manager (__main__.PollSelectorTestCase.test_context_manager)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 320, in test_context_manager
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_get_key (__main__.PollSelectorTestCase.test_get_key)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 232, in test_get_key
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_get_map (__main__.PollSelectorTestCase.test_get_map)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 244, in test_get_map
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_modify (__main__.PollSelectorTestCase.test_modify)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 149, in test_modify
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_modify_unregister (__main__.PollSelectorTestCase.test_modify_unregister)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 203, in test_modify_unregister
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_register (__main__.PollSelectorTestCase.test_register)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 67, in test_register
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_select (__main__.PollSelectorTestCase.test_select)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 273, in test_select
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_select_read_write (__main__.PollSelectorTestCase.test_select_read_write)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 293, in test_select_read_write
    sock1, sock2 = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_selector (__main__.PollSelectorTestCase.test_selector)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 351, in test_selector
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_timeout (__main__.PollSelectorTestCase.test_timeout)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 404, in test_timeout
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister (__main__.PollSelectorTestCase.test_unregister)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 93, in test_unregister
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_fd_close (__main__.PollSelectorTestCase.test_unregister_after_fd_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 107, in test_unregister_after_fd_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_fd_close_and_reuse (__main__.PollSelectorTestCase.test_unregister_after_fd_close_and_reuse)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 120, in test_unregister_after_fd_close_and_reuse
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_socket_close (__main__.PollSelectorTestCase.test_unregister_after_socket_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 137, in test_unregister_after_socket_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_close (__main__.SelectSelectorTestCase.test_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 215, in test_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_context_manager (__main__.SelectSelectorTestCase.test_context_manager)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 320, in test_context_manager
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_get_key (__main__.SelectSelectorTestCase.test_get_key)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 232, in test_get_key
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_get_map (__main__.SelectSelectorTestCase.test_get_map)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 244, in test_get_map
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_modify (__main__.SelectSelectorTestCase.test_modify)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 149, in test_modify
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_register (__main__.SelectSelectorTestCase.test_register)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 67, in test_register
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_select (__main__.SelectSelectorTestCase.test_select)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 273, in test_select
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_select_read_write (__main__.SelectSelectorTestCase.test_select_read_write)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 293, in test_select_read_write
    sock1, sock2 = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_selector (__main__.SelectSelectorTestCase.test_selector)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 351, in test_selector
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_timeout (__main__.SelectSelectorTestCase.test_timeout)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 404, in test_timeout
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister (__main__.SelectSelectorTestCase.test_unregister)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 93, in test_unregister
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_fd_close (__main__.SelectSelectorTestCase.test_unregister_after_fd_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 107, in test_unregister_after_fd_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_fd_close_and_reuse (__main__.SelectSelectorTestCase.test_unregister_after_fd_close_and_reuse)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 120, in test_unregister_after_fd_close_and_reuse
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

======================================================================
ERROR: test_unregister_after_socket_close (__main__.SelectSelectorTestCase.test_unregister_after_socket_close)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_selectors.py", line 137, in test_unregister_after_socket_close
    rd, wr = self.make_socketpair()
  File "/tmp/test_selectors.py", line 58, in make_socketpair
    rd, wr = socketpair()
  File "/pkg/store/python-0/lib/socket.py", line 639, in _fallback_socketpair
    lsock = socket(family, type, proto)
  File "/pkg/store/python-0/lib/socket.py", line 237, in __init__
    _socket.socket.__init__(self, family, type, proto, fileno)
OSError: [Errno 97] Address family not supported by protocol

----------------------------------------------------------------------
Ran 121 tests in Ns

FAILED (errors=44, skipped=71)
