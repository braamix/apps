EEEEEEEEEEEEEEEEEsEEEEEEEEEEEEFEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEFEEEEEEEEEEEEEEEEEE.E.............F.FFFFF....FFF...F..F...FF..F.FF.FF..s..F...sFFFFFFFFFFFFF.F.E....F..F.F.FFFFF.F..FFF..FF.FFFF.F.F.FF.FFFFF..s..F...sFFFFFFFFFFFFF....F..F.F.FFFFF.F..FFF..FF.FFFF.FF.FF.FFFFF..s..F...sFFFFFFFFFFFFF..ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss...F..F.F.FFFFF.F..FFF..FF.FFFF.FF.FF.FFFFF..s..F...sFFFFFFFFFFFFF.
======================================================================
ERROR: test_jump_across_async_with (__main__.JumpTestCase.test_jump_across_async_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2370, in test_jump_across_async_with
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_across_with (__main__.JumpTestCase.test_jump_across_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2362, in test_jump_across_with
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backward_over_async_listcomp (__main__.JumpTestCase.test_jump_backward_over_async_listcomp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2890, in test_jump_backward_over_async_listcomp
    x = [i async for i in asynciter(range(10))]
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backward_over_async_listcomp_v2 (__main__.JumpTestCase.test_jump_backward_over_async_listcomp_v2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2901, in test_jump_backward_over_async_listcomp_v2
    output.append(7)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backward_over_listcomp (__main__.JumpTestCase.test_jump_backward_over_listcomp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2867, in test_jump_backward_over_listcomp
    x = [i for i in range(10)]
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backward_over_listcomp_v2 (__main__.JumpTestCase.test_jump_backward_over_listcomp_v2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2878, in test_jump_backward_over_listcomp_v2
    output.append(7)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backwards_into_try_except_block (__main__.JumpTestCase.test_jump_backwards_into_try_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2635, in test_jump_backwards_into_try_except_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backwards_into_try_finally_block (__main__.JumpTestCase.test_jump_backwards_into_try_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2618, in test_jump_backwards_into_try_finally_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backwards_into_while_block (__main__.JumpTestCase.test_jump_backwards_into_while_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2256, in test_jump_backwards_into_while_block
    while i <= 2:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backwards_out_of_async_with_block (__main__.JumpTestCase.test_jump_backwards_out_of_async_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2282, in test_jump_backwards_out_of_async_with_block
    async with asynctracecontext(output, 2):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backwards_out_of_try_except_block (__main__.JumpTestCase.test_jump_backwards_out_of_try_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2313, in test_jump_backwards_out_of_try_except_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backwards_out_of_try_finally_block (__main__.JumpTestCase.test_jump_backwards_out_of_try_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2296, in test_jump_backwards_out_of_try_finally_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_backwards_out_of_with_block (__main__.JumpTestCase.test_jump_backwards_out_of_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2276, in test_jump_backwards_out_of_with_block
    with tracecontext(output, 2):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_between_async_with_blocks (__main__.JumpTestCase.test_jump_between_async_with_blocks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2749, in test_jump_between_async_with_blocks
    async with asynctracecontext(output, 2):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_between_except_blocks (__main__.JumpTestCase.test_jump_between_except_blocks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2322, in test_jump_between_except_blocks
    1/0
ZeroDivisionError: division by zero

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2324, in test_jump_between_except_blocks
    output.append(4)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_between_except_blocks_2 (__main__.JumpTestCase.test_jump_between_except_blocks_2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2646, in test_jump_between_except_blocks_2
    1/0
ZeroDivisionError: division by zero

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2648, in test_jump_between_except_blocks_2
    output.append(4)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_between_with_blocks (__main__.JumpTestCase.test_jump_between_with_blocks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2741, in test_jump_between_with_blocks
    with tracecontext(output, 2):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_extended_args_unpack_ex_simple (__main__.JumpTestCase.test_jump_extended_args_unpack_ex_simple)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2964, in test_jump_extended_args_unpack_ex_simple
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_extended_args_unpack_ex_tricky (__main__.JumpTestCase.test_jump_extended_args_unpack_ex_tricky)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2971, in test_jump_extended_args_unpack_ex_tricky
    (
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forward_over_async_listcomp (__main__.JumpTestCase.test_jump_forward_over_async_listcomp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2883, in test_jump_forward_over_async_listcomp
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forward_over_listcomp (__main__.JumpTestCase.test_jump_forward_over_listcomp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2858, in test_jump_forward_over_listcomp
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forwards_into_try_except_block (__main__.JumpTestCase.test_jump_forwards_into_try_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2625, in test_jump_forwards_into_try_except_block
    def test_jump_forwards_into_try_except_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forwards_into_try_finally_block (__main__.JumpTestCase.test_jump_forwards_into_try_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2609, in test_jump_forwards_into_try_finally_block
    def test_jump_forwards_into_try_finally_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forwards_into_while_block (__main__.JumpTestCase.test_jump_forwards_into_while_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2247, in test_jump_forwards_into_while_block
    i = 1
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forwards_out_of_async_with_block (__main__.JumpTestCase.test_jump_forwards_out_of_async_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2269, in test_jump_forwards_out_of_async_with_block
    async with asynctracecontext(output, 1):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forwards_out_of_try_except_block (__main__.JumpTestCase.test_jump_forwards_out_of_try_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2303, in test_jump_forwards_out_of_try_except_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forwards_out_of_try_finally_block (__main__.JumpTestCase.test_jump_forwards_out_of_try_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2287, in test_jump_forwards_out_of_try_finally_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_forwards_out_of_with_block (__main__.JumpTestCase.test_jump_forwards_out_of_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2263, in test_jump_forwards_out_of_with_block
    with tracecontext(output, 1):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_from_except_to_finally (__main__.JumpTestCase.test_jump_from_except_to_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2333, in test_jump_from_except_to_finally
    1/0
ZeroDivisionError: division by zero

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2335, in test_jump_from_except_to_finally
    output.append(4)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_in_nested_finally (__main__.JumpTestCase.test_jump_in_nested_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2202, in test_jump_in_nested_finally
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_in_nested_finally_2 (__main__.JumpTestCase.test_jump_in_nested_finally_2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2214, in test_jump_in_nested_finally_2
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_in_nested_finally_3 (__main__.JumpTestCase.test_jump_in_nested_finally_3)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2225, in test_jump_in_nested_finally_3
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_into_finally_block (__main__.JumpTestCase.test_jump_into_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2655, in test_jump_into_finally_block
    def test_jump_into_finally_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_into_finally_block_from_try_block (__main__.JumpTestCase.test_jump_into_finally_block_from_try_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2665, in test_jump_into_finally_block_from_try_block
    output.append(2)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_is_none_backwards (__main__.JumpTestCase.test_jump_is_none_backwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2130, in test_jump_is_none_backwards
    output.append(5)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_is_none_forwards (__main__.JumpTestCase.test_jump_is_none_forwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2117, in test_jump_is_none_forwards
    def test_jump_is_none_forwards(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_is_not_none_backwards (__main__.JumpTestCase.test_jump_is_not_none_backwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2147, in test_jump_is_not_none_backwards
    output.append(5)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_is_not_none_forwards (__main__.JumpTestCase.test_jump_is_not_none_forwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2135, in test_jump_is_not_none_forwards
    x = None
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_or_pop (__main__.JumpTestCase.test_jump_or_pop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2994, in test_jump_or_pop
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_async_for_block_backwards (__main__.JumpTestCase.test_jump_out_of_async_for_block_backwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 59, in wrapper
    return await test(*args, **kwargs, asynciter=wrapped_asynciter)
  File "/tmp/test_sys_settrace.py", line 2183, in test_jump_out_of_async_for_block_backwards
    output.append(4)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_async_for_block_forwards (__main__.JumpTestCase.test_jump_out_of_async_for_block_forwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 59, in wrapper
    return await test(*args, **kwargs, asynciter=wrapped_asynciter)
  File "/tmp/test_sys_settrace.py", line 2173, in test_jump_out_of_async_for_block_forwards
    output.append(3)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_async_with_assignment (__main__.JumpTestCase.test_jump_out_of_async_with_assignment)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2458, in test_jump_out_of_async_with_assignment
    async with asynctracecontext(output, 2) \
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_async_with_block_within_finally_block (__main__.JumpTestCase.test_jump_out_of_async_with_block_within_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2427, in test_jump_out_of_async_with_block_within_finally_block
    async with asynctracecontext(output, 4):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_async_with_block_within_for_block (__main__.JumpTestCase.test_jump_out_of_async_with_block_within_for_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2389, in test_jump_out_of_async_with_block_within_for_block
    async with asynctracecontext(output, 3):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_async_with_block_within_with_block (__main__.JumpTestCase.test_jump_out_of_async_with_block_within_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2407, in test_jump_out_of_async_with_block_within_with_block
    async with asynctracecontext(output, 3):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_bare_except_block (__main__.JumpTestCase.test_jump_out_of_bare_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2723, in test_jump_out_of_bare_except_block
    1/0
ZeroDivisionError: division by zero

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2725, in test_jump_out_of_bare_except_block
    output.append(6)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_block_backwards (__main__.JumpTestCase.test_jump_out_of_block_backwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2163, in test_jump_out_of_block_backwards
    for j in [2]:  # Also tests jumping over a block
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_block_forwards (__main__.JumpTestCase.test_jump_out_of_block_forwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2153, in test_jump_out_of_block_forwards
    output.append(2)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_complex_nested_blocks (__main__.JumpTestCase.test_jump_out_of_complex_nested_blocks)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2440, in test_jump_out_of_complex_nested_blocks
    for k in [1, 2]:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_finally_block (__main__.JumpTestCase.test_jump_out_of_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2675, in test_jump_out_of_finally_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_qualified_except_block (__main__.JumpTestCase.test_jump_out_of_qualified_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2733, in test_jump_out_of_qualified_except_block
    1/0
ZeroDivisionError: division by zero

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2735, in test_jump_out_of_qualified_except_block
    output.append(6)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_with_assignment (__main__.JumpTestCase.test_jump_out_of_with_assignment)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2450, in test_jump_out_of_with_assignment
    with tracecontext(output, 2) \
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_with_block_within_finally_block (__main__.JumpTestCase.test_jump_out_of_with_block_within_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2417, in test_jump_out_of_with_block_within_finally_block
    with tracecontext(output, 4):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_with_block_within_for_block (__main__.JumpTestCase.test_jump_out_of_with_block_within_for_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2380, in test_jump_out_of_with_block_within_for_block
    with tracecontext(output, 3):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_out_of_with_block_within_with_block (__main__.JumpTestCase.test_jump_out_of_with_block_within_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2398, in test_jump_out_of_with_block_within_with_block
    with tracecontext(output, 3):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_over_async_for_block_before_else (__main__.JumpTestCase.test_jump_over_async_for_block_before_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2503, in test_jump_over_async_for_block_before_else
    async def test_jump_over_async_for_block_before_else(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_over_break_in_try_finally_block (__main__.JumpTestCase.test_jump_over_break_in_try_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2480, in test_jump_over_break_in_try_finally_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_over_for_block_before_else (__main__.JumpTestCase.test_jump_over_for_block_before_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2492, in test_jump_over_for_block_before_else
    def test_jump_over_for_block_before_else(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_over_return_in_try_finally_block (__main__.JumpTestCase.test_jump_over_return_in_try_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2466, in test_jump_over_return_in_try_finally_block
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_over_try_except (__main__.JumpTestCase.test_jump_over_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2354, in test_jump_over_try_except
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_simple_backwards (__main__.JumpTestCase.test_jump_simple_backwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2113, in test_jump_simple_backwards
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_simple_forwards (__main__.JumpTestCase.test_jump_simple_forwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2106, in test_jump_simple_forwards
    def test_jump_simple_forwards(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_to_codeless_line (__main__.JumpTestCase.test_jump_to_codeless_line)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2188, in test_jump_to_codeless_line
    def test_jump_to_codeless_line(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_to_firstlineno (__main__.JumpTestCase.test_jump_to_firstlineno)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2823, in test_jump_to_firstlineno
    exec(code, namespace)
  File "<fake module>", line 4, in <module>
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_to_same_line (__main__.JumpTestCase.test_jump_to_same_line)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2195, in test_jump_to_same_line
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_with_null_on_stack_load_attr (__main__.JumpTestCase.test_jump_with_null_on_stack_load_attr)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2947, in test_jump_with_null_on_stack_load_attr
    list.append(
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_with_null_on_stack_load_global (__main__.JumpTestCase.test_jump_with_null_on_stack_load_global)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2908, in test_jump_with_null_on_stack_load_global
    print(
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_with_null_on_stack_push_null (__main__.JumpTestCase.test_jump_with_null_on_stack_push_null)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2928, in test_jump_with_null_on_stack_push_null
    f(
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_jump_within_except_block (__main__.JumpTestCase.test_jump_within_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2344, in test_jump_within_except_block
    1/0
ZeroDivisionError: division by zero

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2346, in test_jump_within_except_block
    output.append(4)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_large_function (__main__.JumpTestCase.test_large_function)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2806, in test_large_function
    self.run_test(f, 2, 1007, [0], warning=(RuntimeWarning, self.unbound_locals))
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "<string>", line 2, in f
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_backwards_into_async_for_block (__main__.JumpTestCase.test_no_jump_backwards_into_async_for_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2580, in test_no_jump_backwards_into_async_for_block
    async for i in asynciter([1, 2]):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_backwards_into_async_with_block (__main__.JumpTestCase.test_no_jump_backwards_into_async_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2604, in test_no_jump_backwards_into_async_with_block
    async with asynctracecontext(output, 1):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_backwards_into_for_block (__main__.JumpTestCase.test_no_jump_backwards_into_for_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2573, in test_no_jump_backwards_into_for_block
    for i in 1, 2:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_backwards_into_with_block (__main__.JumpTestCase.test_no_jump_backwards_into_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2598, in test_no_jump_backwards_into_with_block
    with tracecontext(output, 1):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_forwards_into_async_for_block (__main__.JumpTestCase.test_no_jump_forwards_into_async_for_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2565, in test_no_jump_forwards_into_async_for_block
    async def test_no_jump_forwards_into_async_for_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_forwards_into_async_with_block (__main__.JumpTestCase.test_no_jump_forwards_into_async_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2591, in test_no_jump_forwards_into_async_with_block
    async def test_no_jump_forwards_into_async_with_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_forwards_into_for_block (__main__.JumpTestCase.test_no_jump_forwards_into_for_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2559, in test_no_jump_forwards_into_for_block
    def test_no_jump_forwards_into_for_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_forwards_into_with_block (__main__.JumpTestCase.test_no_jump_forwards_into_with_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2585, in test_no_jump_forwards_into_with_block
    def test_no_jump_forwards_into_with_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_from_exception_event (__main__.JumpTestCase.test_no_jump_from_exception_event)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2046, in run_test
    with contextlib.ExitStack() as stack:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_from_return_event (__main__.JumpTestCase.test_no_jump_from_return_event)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2840, in test_no_jump_from_return_event
    return
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_infinite_while_loop (__main__.JumpTestCase.test_no_jump_infinite_while_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2241, in test_no_jump_infinite_while_loop
    while True:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_into_async_for_block_before_else (__main__.JumpTestCase.test_no_jump_into_async_for_block_before_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2098, in test
    self.run_async_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2074, in run_async_test
    run_no_yield_async_fn(func, output)
  File "/tmp/test/support/__init__.py", line 182, in run_no_yield_async_fn
    coro.send(None)
  File "/tmp/test_sys_settrace.py", line 2782, in test_no_jump_into_async_for_block_before_else
    output.append(6)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_into_bare_except_block (__main__.JumpTestCase.test_no_jump_into_bare_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2681, in test_no_jump_into_bare_except_block
    def test_no_jump_into_bare_except_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_into_bare_except_block_from_try_block (__main__.JumpTestCase.test_no_jump_into_bare_except_block_from_try_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2699, in test_no_jump_into_bare_except_block_from_try_block
    output.append(2)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_into_for_block_before_else (__main__.JumpTestCase.test_no_jump_into_for_block_before_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2771, in test_no_jump_into_for_block_before_else
    output.append(6)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_into_qualified_except_block (__main__.JumpTestCase.test_no_jump_into_qualified_except_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2689, in test_no_jump_into_qualified_except_block
    def test_no_jump_into_qualified_except_block(output):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_into_qualified_except_block_from_try_block (__main__.JumpTestCase.test_no_jump_into_qualified_except_block_from_try_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2710, in test_no_jump_into_qualified_except_block_from_try_block
    output.append(2)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_over_return_out_of_finally_block (__main__.JumpTestCase.test_no_jump_over_return_out_of_finally_block)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2759, in test_no_jump_over_return_out_of_finally_block
    output.append(4)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_to_except_1 (__main__.JumpTestCase.test_no_jump_to_except_1)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2528, in test_no_jump_to_except_1
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_to_except_2 (__main__.JumpTestCase.test_no_jump_to_except_2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2536, in test_no_jump_to_except_2
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_to_except_3 (__main__.JumpTestCase.test_no_jump_to_except_3)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2544, in test_no_jump_to_except_3
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_to_except_4 (__main__.JumpTestCase.test_no_jump_to_except_4)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2552, in test_no_jump_to_except_4
    try:
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_to_non_integers (__main__.JumpTestCase.test_no_jump_to_non_integers)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
TypeError: unsupported operand type(s) for +: 'int' and 'str'

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2787, in test_no_jump_to_non_integers
    self.run_test(no_jump_to_non_integers, 2, "Spam", [True])
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2002, in no_jump_to_non_integers
    try:
  File "/tmp/test_sys_settrace.py", line 1996, in trace
    frame.f_lineno = self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_too_far_backwards (__main__.JumpTestCase.test_no_jump_too_far_backwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2522, in test_no_jump_too_far_backwards
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_too_far_forwards (__main__.JumpTestCase.test_no_jump_too_far_forwards)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2054, in run_test
    func(output)
  File "/tmp/test_sys_settrace.py", line 2517, in test_no_jump_too_far_forwards
    output.append(1)
  File "/tmp/test_sys_settrace.py", line 1994, in trace
    frame.f_lineno = self.firstLine + self.jumpTo
Exception

======================================================================
ERROR: test_no_jump_without_trace_function (__main__.JumpTestCase.test_no_jump_without_trace_function)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2792, in test_no_jump_without_trace_function
    no_jump_without_trace_function()
  File "/tmp/test_sys_settrace.py", line 2012, in no_jump_without_trace_function
    previous_frame.f_lineno = previous_frame.f_lineno
Exception

======================================================================
ERROR: test_exception (__main__.RaisingTraceFuncTestCase.test_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1897, in test_exception
    self.run_test_for_event('exception')
  File "/tmp/test_sys_settrace.py", line 1880, in run_test_for_event
    try:
  File "/tmp/test_sys_settrace.py", line 1860, in trace
    raise ValueError # just something that isn't RuntimeError
ValueError

======================================================================
ERROR: test_trace_lots_of_globals (__main__.TestExtendedArgs.test_trace_lots_of_globals)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 3041, in test_trace_lots_of_globals
    self.assertEqual(counts, {'call': 1, 'line': count * 2 + 1, 'return': 1})
  File "/pkg/store/python-0/lib/unittest/case.py", line 949, in assertEqual
    assertion_func(first, second, msg=msg)
  File "/pkg/store/python-0/lib/unittest/case.py", line 1245, in assertDictEqual
    pprint.pformat(d1).splitlines(),
  File "/pkg/store/python-0/lib/pprint.py", line 63, in pformat
    underscore_numbers=underscore_numbers).pformat(object)
  File "/pkg/store/python-0/lib/pprint.py", line 179, in pformat
    self._format(object, sio, 0, 0, {}, 0)
  File "/pkg/store/python-0/lib/pprint.py", line 196, in _format
    rep = self._repr(object, context, level)
  File "/pkg/store/python-0/lib/pprint.py", line 625, in _repr
    repr, readable, recursive = self.format(object, context.copy(),
  File "/pkg/store/python-0/lib/pprint.py", line 638, in format
    return self._safe_repr(object, context, maxlevels, level)
  File "/pkg/store/python-0/lib/pprint.py", line 831, in _safe_repr
    items = sorted(object.items(), key=_safe_tuple)
TypeError: '<' not supported between instances of '_safe_key' and '_safe_key'

======================================================================
FAIL: test_jump_from_yield (__main__.JumpTestCase.test_jump_from_yield)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2057, in run_test
    self.compare_jump_output(expected, output)
  File "/tmp/test_sys_settrace.py", line 2032, in compare_jump_output
    self.fail( "Outputs don't match:\n" +
AssertionError: Outputs don't match:
Expected: [2, 2, 5]
Received: [2, 5]

======================================================================
FAIL: test_no_jump_from_call (__main__.JumpTestCase.test_no_jump_from_call)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 2086, in test
    self.run_test(func, jumpFrom, jumpTo, expected,
  File "/tmp/test_sys_settrace.py", line 2046, in run_test
    with contextlib.ExitStack() as stack:
  File "/pkg/store/python-0/lib/contextlib.py", line 676, in __exit__
    raise exc
  File "/pkg/store/python-0/lib/contextlib.py", line 661, in __exit__
    if cb(*exc_details):
AssertionError: ValueError not raised

======================================================================
FAIL: test_07_raise (__main__.SkipLineEventsTraceTestCase.test_07_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 435, in test_07_raise
    self.run_test(test_raise)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (-3, 'call')
  (-2, 'exception')
  (-2, 'return')
  (2, 'exception')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_09_settrace_and_raise (__main__.SkipLineEventsTraceTestCase.test_09_settrace_and_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 440, in test_09_settrace_and_raise
    self.run_test2(settrace_and_raise)
  File "/tmp/test_sys_settrace.py", line 401, in run_test2
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (2, 'exception')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_10_ireturn (__main__.SkipLineEventsTraceTestCase.test_10_ireturn)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 442, in test_10_ireturn
    self.run_test(ireturn_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (4, 'return')
?  ^

+ (6, 'return')
?  ^


======================================================================
FAIL: test_11_tightloop (__main__.SkipLineEventsTraceTestCase.test_11_tightloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 444, in test_11_tightloop
    self.run_test(tightloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (5, 'exception')
- (7, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_12_tighterloop (__main__.SkipLineEventsTraceTestCase.test_12_tighterloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 446, in test_12_tighterloop
    self.run_test(tighterloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (4, 'exception')
- (6, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_13_genexp (__main__.SkipLineEventsTraceTestCase.test_13_genexp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 449, in test_13_genexp
    self.run_test(generator_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (-6, 'call')
  (-4, 'return')
- (-4, 'call')
- (-4, 'exception')
- (-1, 'return')
  (5, 'return')

======================================================================
FAIL: test_18_except_with_name (__main__.SkipLineEventsTraceTestCase.test_18_except_with_name)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 534, in test_18_except_with_name
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (5, 'exception')
+ (4, 'exception')
- (9, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_19_except_with_finally (__main__.SkipLineEventsTraceTestCase.test_19_except_with_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 556, in test_19_except_with_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (2, 'exception')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_20_async_for_loop (__main__.SkipLineEventsTraceTestCase.test_20_async_for_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 640, in test_20_async_for_loop
    self.compare_events(doit_async.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (-12, 'call')
  (-11, 'return')
  (-9, 'call')
  (-8, 'return')
  (-6, 'call')
  (-4, 'return')
- (1, 'exception')
  (-6, 'call')
  (-4, 'return')
- (1, 'exception')
  (-6, 'call')
  (-4, 'return')
- (1, 'exception')
  (-6, 'call')
  (-4, 'exception')
  (-2, 'exception')
+ (-3, 'exception')
- (-2, 'return')
?   ^

+ (-3, 'return')
?   ^

  (1, 'exception')
  (3, 'return')

======================================================================
FAIL: test_break_to_break (__main__.SkipLineEventsTraceTestCase.test_break_to_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1029, in test_break_to_break
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (5, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_class_creation_with_decorator (__main__.SkipLineEventsTraceTestCase.test_class_creation_with_decorator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1606, in test_class_creation_with_decorator
    self.run_and_compare(func, [
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'call')
  (4, 'return')
  (1, 'call')
  (4, 'return')
- (6, 'call')
?  ^

+ (10, 'call')
?  ^^

  (11, 'return')
  (2, 'call')
  (3, 'return')
  (2, 'call')
  (3, 'return')
  (10, 'return')

======================================================================
FAIL: test_early_exit_with (__main__.SkipLineEventsTraceTestCase.test_early_exit_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1271, in test_early_exit_with
    self.run_and_compare(func_return,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (-11, 'call')
  (-10, 'return')
  (-9, 'call')
  (-8, 'return')
- (1, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_finally_with_conditional (__main__.SkipLineEventsTraceTestCase.test_finally_with_conditional)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 961, in test_finally_with_conditional
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (2, 'exception')
  (10, 'return')

======================================================================
FAIL: test_if_false_in_try_except (__main__.SkipLineEventsTraceTestCase.test_if_false_in_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1138, in test_if_false_in_try_except
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (2, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_if_in_if_in_if (__main__.SkipLineEventsTraceTestCase.test_if_in_if_in_if)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1232, in test_if_in_if_in_if
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (2, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_implicit_return_in_class (__main__.SkipLineEventsTraceTestCase.test_implicit_return_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1153, in test_implicit_return_in_class
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'call')
- (3, 'return')
?  ^

+ (5, 'return')
?  ^

  (1, 'return')

======================================================================
FAIL: test_nested_ifs (__main__.SkipLineEventsTraceTestCase.test_nested_ifs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1050, in test_nested_ifs
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (4, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_nested_ifs_with_and (__main__.SkipLineEventsTraceTestCase.test_nested_ifs_with_and)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1074, in test_nested_ifs_with_and
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (3, 'return')
?  ^

+ (9, 'return')
?  ^


======================================================================
FAIL: test_return_through_finally (__main__.SkipLineEventsTraceTestCase.test_return_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 911, in test_return_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (4, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_tracing_exception_raised_in_with (__main__.SkipLineEventsTraceTestCase.test_tracing_exception_raised_in_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1357, in test_tracing_exception_raised_in_with
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (-5, 'call')
  (-4, 'return')
  (3, 'exception')
  (-3, 'call')
  (-2, 'return')
+ (2, 'exception')
- (5, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_no_exception (__main__.SkipLineEventsTraceTestCase.test_try_except_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 738, in test_try_except_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_star_exception_caught (__main__.SkipLineEventsTraceTestCase.test_try_except_star_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1435, in test_try_except_star_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'exception')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_exception_not_caught (__main__.SkipLineEventsTraceTestCase.test_try_except_star_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1478, in test_try_except_star_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (2, 'exception')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_caught (__main__.SkipLineEventsTraceTestCase.test_try_except_star_named_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1457, in test_try_except_star_named_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'exception')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_not_caught (__main__.SkipLineEventsTraceTestCase.test_try_except_star_named_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1500, in test_try_except_star_named_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (2, 'exception')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_no_exception (__main__.SkipLineEventsTraceTestCase.test_try_except_star_named_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1415, in test_try_except_star_named_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_nested (__main__.SkipLineEventsTraceTestCase.test_try_except_star_nested)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1534, in test_try_except_star_nested
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (11, 'exception')
+ (2, 'exception')
  (14, 'exception')
  (19, 'return')

======================================================================
FAIL: test_try_except_star_no_exception (__main__.SkipLineEventsTraceTestCase.test_try_except_star_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1392, in test_try_except_star_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_with_wrong_type (__main__.SkipLineEventsTraceTestCase.test_try_except_with_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 932, in test_try_except_with_wrong_type
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (2, 'exception')
+ (2, 'exception')
  (10, 'return')

======================================================================
FAIL: test_try_exception_in_else (__main__.SkipLineEventsTraceTestCase.test_try_exception_in_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 767, in test_try_exception_in_else
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (8, 'exception')
+ (2, 'exception')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_in_try (__main__.SkipLineEventsTraceTestCase.test_try_in_try)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1173, in test_try_in_try
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
- (3, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_in_try_with_exception (__main__.SkipLineEventsTraceTestCase.test_try_in_try_with_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1191, in test_try_in_try_with_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 1795, in compare_events
    super().compare_events(line_offset, events, skip_line_events)
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (3, 'exception')
+ (2, 'exception')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_reentrancy (__main__.TestEdgeCases.test_reentrancy)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 3066, in test_reentrancy
    self.assertEqual(sys.gettrace(), bar)
AssertionError: <function TestEdgeCases.test_reentrancy.<locals>.foo> != <function TestEdgeCases.test_reentrancy.<locals>.bar>

======================================================================
FAIL: test_02_arigo2 (__main__.TestLinesAfterTraceStarted.test_02_arigo2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 425, in test_02_arigo2
    self.run_test(arigo_example2)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
  (7, 'line')
  (7, 'return')

======================================================================
FAIL: test_05_no_pop_tops (__main__.TestLinesAfterTraceStarted.test_05_no_pop_tops)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 431, in test_05_no_pop_tops
    self.run_test(no_pop_tops)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (6, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
  (2, 'line')
  (2, 'return')

======================================================================
FAIL: test_07_raise (__main__.TestLinesAfterTraceStarted.test_07_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 435, in test_07_raise
    self.run_test(test_raise)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'exception')
  (-2, 'return')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_09_settrace_and_raise (__main__.TestLinesAfterTraceStarted.test_09_settrace_and_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 440, in test_09_settrace_and_raise
    self.run_test2(settrace_and_raise)
  File "/tmp/test_sys_settrace.py", line 401, in run_test2
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (2, 'exception')
  (3, 'line')
  (4, 'line')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_10_ireturn (__main__.TestLinesAfterTraceStarted.test_10_ireturn)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 442, in test_10_ireturn
    self.run_test(ireturn_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
- (4, 'return')
?  ^

+ (6, 'return')
?  ^


======================================================================
FAIL: test_11_tightloop (__main__.TestLinesAfterTraceStarted.test_11_tightloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 444, in test_11_tightloop
    self.run_test(tightloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (4, 'line')
+ (4, 'line')
  (5, 'line')
  (4, 'line')
+ (4, 'line')
  (5, 'line')
+ (4, 'line')
  (4, 'line')
  (5, 'line')
  (5, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_12_tighterloop (__main__.TestLinesAfterTraceStarted.test_12_tighterloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 446, in test_12_tighterloop
    self.run_test(tighterloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'exception')
  (5, 'line')
  (6, 'line')
- (6, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_13_genexp (__main__.TestLinesAfterTraceStarted.test_13_genexp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 449, in test_13_genexp
    self.run_test(generator_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
  (-4, 'return')
- (-4, 'call')
- (-4, 'exception')
- (-1, 'line')
- (-1, 'return')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (5, 'return')

======================================================================
FAIL: test_15_loops (__main__.TestLinesAfterTraceStarted.test_15_loops)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 493, in test_15_loops
    self.run_and_compare(
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (3, 'line')
+ (3, 'line')
  (4, 'line')
  (3, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_18_except_with_name (__main__.TestLinesAfterTraceStarted.test_18_except_with_name)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 534, in test_18_except_with_name
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
  (5, 'line')
+ (5, 'exception')
+ (4, 'line')
+ (4, 'exception')
  (8, 'line')
  (9, 'line')
- (9, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_19_except_with_finally (__main__.TestLinesAfterTraceStarted.test_19_except_with_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 556, in test_19_except_with_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (5, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_20_async_for_loop (__main__.TestLinesAfterTraceStarted.test_20_async_for_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 640, in test_20_async_for_loop
    self.compare_events(doit_async.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (-12, 'call')
  (-11, 'line')
  (-11, 'return')
  (-9, 'call')
  (-8, 'line')
  (-8, 'return')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
  (-4, 'exception')
  (-3, 'line')
  (-2, 'line')
  (-2, 'exception')
+ (-3, 'line')
+ (-3, 'exception')
- (-2, 'return')
?   ^

+ (-3, 'return')
?   ^

  (1, 'exception')
  (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_break_through_finally (__main__.TestLinesAfterTraceStarted.test_break_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 849, in test_break_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (8, 'line')
+ (4, 'line')
  (10, 'line')
+ (4, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
+ (4, 'line')
+ (10, 'line')
  (7, 'line')
- (10, 'line')
?  ^^

+ (2, 'line')
?  ^

  (13, 'line')
  (13, 'return')

======================================================================
FAIL: test_break_to_break (__main__.TestLinesAfterTraceStarted.test_break_to_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1029, in test_break_to_break
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
- (5, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_break_to_continue2 (__main__.TestLinesAfterTraceStarted.test_break_to_continue2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1009, in test_break_to_continue2
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (3, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_class_creation_with_decorator (__main__.TestLinesAfterTraceStarted.test_class_creation_with_decorator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1606, in test_class_creation_with_decorator
    self.run_and_compare(func, [
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (6, 'line')
  (1, 'call')
  (2, 'line')
  (4, 'line')
  (4, 'return')
  (7, 'line')
  (8, 'line')
  (7, 'line')
  (1, 'call')
  (2, 'line')
  (4, 'line')
  (4, 'return')
  (10, 'line')
- (6, 'call')
?  ^

+ (10, 'call')
?  ^^

- (6, 'line')
  (11, 'line')
  (11, 'return')
- (7, 'line')
  (2, 'call')
  (3, 'line')
  (3, 'return')
- (6, 'line')
  (2, 'call')
  (3, 'line')
  (3, 'return')
- (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_class_creation_with_docstrings (__main__.TestLinesAfterTraceStarted.test_class_creation_with_docstrings)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1582, in test_class_creation_with_docstrings
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (1, 'call')
- (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'return')
  (1, 'return')

======================================================================
FAIL: test_continue_through_finally (__main__.TestLinesAfterTraceStarted.test_continue_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 884, in test_continue_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (8, 'line')
+ (4, 'line')
  (10, 'line')
+ (4, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
+ (4, 'line')
+ (10, 'line')
  (7, 'line')
- (10, 'line')
- (3, 'line')
?  ^

+ (2, 'line')
?  ^

  (13, 'line')
  (13, 'return')

======================================================================
FAIL: test_early_exit_with (__main__.TestLinesAfterTraceStarted.test_early_exit_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1256, in test_early_exit_with
    self.run_and_compare(func_break,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-5, 'call')
  (-4, 'line')
  (-4, 'return')
- (3, 'line')
- (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'return')
+ (3, 'line')
  (4, 'line')
  (4, 'return')

======================================================================
FAIL: test_finally_with_conditional (__main__.TestLinesAfterTraceStarted.test_finally_with_conditional)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 961, in test_finally_with_conditional
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (5, 'line')
  (6, 'line')
+ (2, 'line')
+ (2, 'exception')
  (8, 'line')
  (9, 'line')
  (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_if_break (__main__.TestLinesAfterTraceStarted.test_if_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 819, in test_if_break
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (2, 'line')
+ (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (8, 'line')
  (8, 'return')

======================================================================
FAIL: test_if_false_in_try_except (__main__.TestLinesAfterTraceStarted.test_if_false_in_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1138, in test_if_false_in_try_except
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
- (2, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_if_in_if_in_if (__main__.TestLinesAfterTraceStarted.test_if_in_if_in_if)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1232, in test_if_in_if_in_if
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
- (2, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_implicit_return_in_class (__main__.TestLinesAfterTraceStarted.test_implicit_return_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1153, in test_implicit_return_in_class
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (1, 'call')
- (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
- (3, 'return')
?  ^

+ (5, 'return')
?  ^

  (1, 'return')

======================================================================
FAIL: test_loop_in_try_except (__main__.TestLinesAfterTraceStarted.test_loop_in_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 713, in test_loop_in_try_except
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (1, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_nested_ifs (__main__.TestLinesAfterTraceStarted.test_nested_ifs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1050, in test_nested_ifs
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (2, 'line')
- (4, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_nested_ifs_with_and (__main__.TestLinesAfterTraceStarted.test_nested_ifs_with_and)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1074, in test_nested_ifs_with_and
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
+ (1, 'line')
- (3, 'return')
?  ^

+ (9, 'return')
?  ^


======================================================================
FAIL: test_return_through_finally (__main__.TestLinesAfterTraceStarted.test_return_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 911, in test_return_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (4, 'line')
+ (2, 'line')
- (4, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_tracing_exception_raised_in_with (__main__.TestLinesAfterTraceStarted.test_tracing_exception_raised_in_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1357, in test_tracing_exception_raised_in_with
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-5, 'call')
  (-4, 'line')
  (-4, 'return')
  (3, 'line')
  (3, 'exception')
- (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'return')
+ (2, 'exception')
  (4, 'line')
  (5, 'line')
- (5, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_no_exception (__main__.TestLinesAfterTraceStarted.test_try_except_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 738, in test_try_except_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
  (7, 'line')
  (10, 'line')
  (11, 'line')
+ (1, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_star_exception_caught (__main__.TestLinesAfterTraceStarted.test_try_except_star_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1435, in test_try_except_star_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_exception_not_caught (__main__.TestLinesAfterTraceStarted.test_try_except_star_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1478, in test_try_except_star_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_caught (__main__.TestLinesAfterTraceStarted.test_try_except_star_named_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1457, in test_try_except_star_named_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_not_caught (__main__.TestLinesAfterTraceStarted.test_try_except_star_named_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1500, in test_try_except_star_named_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_no_exception (__main__.TestLinesAfterTraceStarted.test_try_except_star_named_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1415, in test_try_except_star_named_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_nested (__main__.TestLinesAfterTraceStarted.test_try_except_star_nested)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1534, in test_try_except_star_nested
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (3, 'line')
  (3, 'exception')
  (6, 'line')
  (7, 'line')
+ (6, 'line')
  (8, 'line')
  (10, 'line')
  (11, 'line')
+ (11, 'exception')
+ (2, 'line')
+ (2, 'exception')
  (12, 'line')
  (13, 'line')
  (14, 'line')
  (14, 'exception')
  (15, 'line')
  (17, 'line')
  (18, 'line')
+ (17, 'line')
+ (13, 'line')
+ (12, 'line')
+ (1, 'line')
  (19, 'line')
  (19, 'return')

======================================================================
FAIL: test_try_except_star_no_exception (__main__.TestLinesAfterTraceStarted.test_try_except_star_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1392, in test_try_except_star_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
  (7, 'line')
  (10, 'line')
  (11, 'line')
+ (1, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_with_wrong_type (__main__.TestLinesAfterTraceStarted.test_try_except_with_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 932, in test_try_except_with_wrong_type
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (7, 'line')
+ (2, 'line')
+ (2, 'exception')
  (8, 'line')
  (9, 'line')
  (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_try_exception_in_else (__main__.TestLinesAfterTraceStarted.test_try_exception_in_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 767, in test_try_exception_in_else
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
  (7, 'line')
  (8, 'line')
  (8, 'exception')
  (10, 'line')
+ (2, 'line')
+ (2, 'exception')
  (11, 'line')
  (12, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_in_try (__main__.TestLinesAfterTraceStarted.test_try_in_try)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1173, in test_try_in_try
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
+ (1, 'line')
- (3, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_in_try_with_exception (__main__.TestLinesAfterTraceStarted.test_try_in_try_with_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1191, in test_try_in_try_with_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_02_arigo2 (__main__.TestSetLocalTrace.test_02_arigo2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 425, in test_02_arigo2
    self.run_test(arigo_example2)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
  (7, 'line')
  (7, 'return')

======================================================================
FAIL: test_05_no_pop_tops (__main__.TestSetLocalTrace.test_05_no_pop_tops)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 431, in test_05_no_pop_tops
    self.run_test(no_pop_tops)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (6, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
  (2, 'line')
  (2, 'return')

======================================================================
FAIL: test_07_raise (__main__.TestSetLocalTrace.test_07_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 435, in test_07_raise
    self.run_test(test_raise)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'exception')
  (-2, 'return')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_09_settrace_and_raise (__main__.TestSetLocalTrace.test_09_settrace_and_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 440, in test_09_settrace_and_raise
    self.run_test2(settrace_and_raise)
  File "/tmp/test_sys_settrace.py", line 401, in run_test2
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (2, 'exception')
  (3, 'line')
  (4, 'line')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_10_ireturn (__main__.TestSetLocalTrace.test_10_ireturn)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 442, in test_10_ireturn
    self.run_test(ireturn_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
- (4, 'return')
?  ^

+ (6, 'return')
?  ^


======================================================================
FAIL: test_11_tightloop (__main__.TestSetLocalTrace.test_11_tightloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 444, in test_11_tightloop
    self.run_test(tightloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (4, 'line')
+ (4, 'line')
  (5, 'line')
  (4, 'line')
+ (4, 'line')
  (5, 'line')
+ (4, 'line')
  (4, 'line')
  (5, 'line')
  (5, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_12_tighterloop (__main__.TestSetLocalTrace.test_12_tighterloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 446, in test_12_tighterloop
    self.run_test(tighterloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'exception')
  (5, 'line')
  (6, 'line')
- (6, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_13_genexp (__main__.TestSetLocalTrace.test_13_genexp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 449, in test_13_genexp
    self.run_test(generator_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
  (-4, 'return')
- (-4, 'call')
- (-4, 'exception')
- (-1, 'line')
- (-1, 'return')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (5, 'return')

======================================================================
FAIL: test_15_loops (__main__.TestSetLocalTrace.test_15_loops)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 493, in test_15_loops
    self.run_and_compare(
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (3, 'line')
+ (3, 'line')
  (4, 'line')
  (3, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_18_except_with_name (__main__.TestSetLocalTrace.test_18_except_with_name)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 534, in test_18_except_with_name
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
  (5, 'line')
+ (5, 'exception')
+ (4, 'line')
+ (4, 'exception')
  (8, 'line')
  (9, 'line')
- (9, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_19_except_with_finally (__main__.TestSetLocalTrace.test_19_except_with_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 556, in test_19_except_with_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (5, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_20_async_for_loop (__main__.TestSetLocalTrace.test_20_async_for_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 640, in test_20_async_for_loop
    self.compare_events(doit_async.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (-12, 'call')
  (-11, 'line')
  (-11, 'return')
  (-9, 'call')
  (-8, 'line')
  (-8, 'return')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
  (-4, 'exception')
  (-3, 'line')
  (-2, 'line')
  (-2, 'exception')
+ (-3, 'line')
+ (-3, 'exception')
- (-2, 'return')
?   ^

+ (-3, 'return')
?   ^

  (1, 'exception')
  (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_break_through_finally (__main__.TestSetLocalTrace.test_break_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 849, in test_break_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (8, 'line')
+ (4, 'line')
  (10, 'line')
+ (4, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
+ (4, 'line')
+ (10, 'line')
  (7, 'line')
- (10, 'line')
?  ^^

+ (2, 'line')
?  ^

  (13, 'line')
  (13, 'return')

======================================================================
FAIL: test_break_to_break (__main__.TestSetLocalTrace.test_break_to_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1029, in test_break_to_break
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
- (5, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_break_to_continue2 (__main__.TestSetLocalTrace.test_break_to_continue2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1009, in test_break_to_continue2
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (3, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_class_creation_with_decorator (__main__.TestSetLocalTrace.test_class_creation_with_decorator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1606, in test_class_creation_with_decorator
    self.run_and_compare(func, [
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (6, 'line')
  (1, 'call')
  (2, 'line')
  (4, 'line')
  (4, 'return')
  (7, 'line')
  (8, 'line')
  (7, 'line')
  (1, 'call')
  (2, 'line')
  (4, 'line')
  (4, 'return')
  (10, 'line')
- (6, 'call')
?  ^

+ (10, 'call')
?  ^^

- (6, 'line')
  (11, 'line')
  (11, 'return')
- (7, 'line')
  (2, 'call')
  (3, 'line')
  (3, 'return')
- (6, 'line')
  (2, 'call')
  (3, 'line')
  (3, 'return')
- (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_class_creation_with_docstrings (__main__.TestSetLocalTrace.test_class_creation_with_docstrings)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1582, in test_class_creation_with_docstrings
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (1, 'call')
- (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'return')
  (1, 'return')

======================================================================
FAIL: test_continue_through_finally (__main__.TestSetLocalTrace.test_continue_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 884, in test_continue_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (8, 'line')
+ (4, 'line')
  (10, 'line')
+ (4, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
+ (4, 'line')
+ (10, 'line')
  (7, 'line')
- (10, 'line')
- (3, 'line')
?  ^

+ (2, 'line')
?  ^

  (13, 'line')
  (13, 'return')

======================================================================
FAIL: test_early_exit_with (__main__.TestSetLocalTrace.test_early_exit_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1256, in test_early_exit_with
    self.run_and_compare(func_break,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-5, 'call')
  (-4, 'line')
  (-4, 'return')
- (3, 'line')
- (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'return')
+ (3, 'line')
  (4, 'line')
  (4, 'return')

======================================================================
FAIL: test_finally_with_conditional (__main__.TestSetLocalTrace.test_finally_with_conditional)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 961, in test_finally_with_conditional
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (5, 'line')
  (6, 'line')
+ (2, 'line')
+ (2, 'exception')
  (8, 'line')
  (9, 'line')
  (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_if_break (__main__.TestSetLocalTrace.test_if_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 819, in test_if_break
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (2, 'line')
+ (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (8, 'line')
  (8, 'return')

======================================================================
FAIL: test_if_false_in_try_except (__main__.TestSetLocalTrace.test_if_false_in_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1138, in test_if_false_in_try_except
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
- (2, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_if_in_if_in_if (__main__.TestSetLocalTrace.test_if_in_if_in_if)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1232, in test_if_in_if_in_if
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
- (2, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_implicit_return_in_class (__main__.TestSetLocalTrace.test_implicit_return_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1153, in test_implicit_return_in_class
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (1, 'call')
- (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
- (3, 'return')
?  ^

+ (5, 'return')
?  ^

  (1, 'return')

======================================================================
FAIL: test_loop_in_try_except (__main__.TestSetLocalTrace.test_loop_in_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 713, in test_loop_in_try_except
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (1, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_nested_ifs (__main__.TestSetLocalTrace.test_nested_ifs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1050, in test_nested_ifs
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (2, 'line')
- (4, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_nested_ifs_with_and (__main__.TestSetLocalTrace.test_nested_ifs_with_and)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1074, in test_nested_ifs_with_and
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
+ (1, 'line')
- (3, 'return')
?  ^

+ (9, 'return')
?  ^


======================================================================
FAIL: test_return_through_finally (__main__.TestSetLocalTrace.test_return_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 911, in test_return_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (4, 'line')
+ (2, 'line')
- (4, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_tracing_exception_raised_in_with (__main__.TestSetLocalTrace.test_tracing_exception_raised_in_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1357, in test_tracing_exception_raised_in_with
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-5, 'call')
  (-4, 'line')
  (-4, 'return')
  (3, 'line')
  (3, 'exception')
- (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'return')
+ (2, 'exception')
  (4, 'line')
  (5, 'line')
- (5, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_no_exception (__main__.TestSetLocalTrace.test_try_except_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 738, in test_try_except_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
  (7, 'line')
  (10, 'line')
  (11, 'line')
+ (1, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_star_exception_caught (__main__.TestSetLocalTrace.test_try_except_star_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1435, in test_try_except_star_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_exception_not_caught (__main__.TestSetLocalTrace.test_try_except_star_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1478, in test_try_except_star_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_caught (__main__.TestSetLocalTrace.test_try_except_star_named_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1457, in test_try_except_star_named_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_not_caught (__main__.TestSetLocalTrace.test_try_except_star_named_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1500, in test_try_except_star_named_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_no_exception (__main__.TestSetLocalTrace.test_try_except_star_named_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1415, in test_try_except_star_named_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_nested (__main__.TestSetLocalTrace.test_try_except_star_nested)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1534, in test_try_except_star_nested
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (3, 'line')
  (3, 'exception')
  (6, 'line')
  (7, 'line')
+ (6, 'line')
  (8, 'line')
  (10, 'line')
  (11, 'line')
+ (11, 'exception')
+ (2, 'line')
+ (2, 'exception')
  (12, 'line')
  (13, 'line')
  (14, 'line')
  (14, 'exception')
  (15, 'line')
  (17, 'line')
  (18, 'line')
+ (17, 'line')
+ (13, 'line')
+ (12, 'line')
+ (1, 'line')
  (19, 'line')
  (19, 'return')

======================================================================
FAIL: test_try_except_star_no_exception (__main__.TestSetLocalTrace.test_try_except_star_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1392, in test_try_except_star_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
  (7, 'line')
  (10, 'line')
  (11, 'line')
+ (1, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_with_wrong_type (__main__.TestSetLocalTrace.test_try_except_with_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 932, in test_try_except_with_wrong_type
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (7, 'line')
+ (2, 'line')
+ (2, 'exception')
  (8, 'line')
  (9, 'line')
  (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_try_exception_in_else (__main__.TestSetLocalTrace.test_try_exception_in_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 767, in test_try_exception_in_else
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
  (7, 'line')
  (8, 'line')
  (8, 'exception')
  (10, 'line')
+ (2, 'line')
+ (2, 'exception')
  (11, 'line')
  (12, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_in_try (__main__.TestSetLocalTrace.test_try_in_try)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1173, in test_try_in_try
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
+ (1, 'line')
- (3, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_in_try_with_exception (__main__.TestSetLocalTrace.test_try_in_try_with_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1191, in test_try_in_try_with_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_02_arigo2 (__main__.TraceTestCase.test_02_arigo2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 425, in test_02_arigo2
    self.run_test(arigo_example2)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
  (7, 'line')
  (7, 'return')

======================================================================
FAIL: test_05_no_pop_tops (__main__.TraceTestCase.test_05_no_pop_tops)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 431, in test_05_no_pop_tops
    self.run_test(no_pop_tops)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (6, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
  (2, 'line')
  (2, 'return')

======================================================================
FAIL: test_07_raise (__main__.TraceTestCase.test_07_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 435, in test_07_raise
    self.run_test(test_raise)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'exception')
  (-2, 'return')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_09_settrace_and_raise (__main__.TraceTestCase.test_09_settrace_and_raise)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 440, in test_09_settrace_and_raise
    self.run_test2(settrace_and_raise)
  File "/tmp/test_sys_settrace.py", line 401, in run_test2
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (2, 'exception')
  (3, 'line')
  (4, 'line')
- (4, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_10_ireturn (__main__.TraceTestCase.test_10_ireturn)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 442, in test_10_ireturn
    self.run_test(ireturn_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
- (4, 'return')
?  ^

+ (6, 'return')
?  ^


======================================================================
FAIL: test_11_tightloop (__main__.TraceTestCase.test_11_tightloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 444, in test_11_tightloop
    self.run_test(tightloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (4, 'line')
+ (4, 'line')
  (5, 'line')
  (4, 'line')
+ (4, 'line')
  (5, 'line')
+ (4, 'line')
  (4, 'line')
  (5, 'line')
  (5, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_12_tighterloop (__main__.TraceTestCase.test_12_tighterloop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 446, in test_12_tighterloop
    self.run_test(tighterloop_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'line')
  (4, 'exception')
  (5, 'line')
  (6, 'line')
- (6, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_13_genexp (__main__.TraceTestCase.test_13_genexp)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 449, in test_13_genexp
    self.run_test(generator_example)
  File "/tmp/test_sys_settrace.py", line 395, in run_test
    self.run_and_compare(func, func.events)
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
  (-4, 'return')
- (-4, 'call')
- (-4, 'exception')
- (-1, 'line')
- (-1, 'return')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (6, 'line')
  (5, 'line')
  (5, 'return')

======================================================================
FAIL: test_15_loops (__main__.TraceTestCase.test_15_loops)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 493, in test_15_loops
    self.run_and_compare(
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (3, 'line')
+ (3, 'line')
  (4, 'line')
  (3, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_18_except_with_name (__main__.TraceTestCase.test_18_except_with_name)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 534, in test_18_except_with_name
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
  (5, 'line')
+ (5, 'exception')
+ (4, 'line')
+ (4, 'exception')
  (8, 'line')
  (9, 'line')
- (9, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_19_except_with_finally (__main__.TraceTestCase.test_19_except_with_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 556, in test_19_except_with_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (5, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_20_async_for_loop (__main__.TraceTestCase.test_20_async_for_loop)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 640, in test_20_async_for_loop
    self.compare_events(doit_async.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (-12, 'call')
  (-11, 'line')
  (-11, 'return')
  (-9, 'call')
  (-8, 'line')
  (-8, 'return')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
+ (-5, 'line')
+ (-4, 'line')
  (-4, 'return')
- (1, 'exception')
  (2, 'line')
  (1, 'line')
  (-6, 'call')
  (-5, 'line')
  (-4, 'line')
  (-4, 'exception')
  (-3, 'line')
  (-2, 'line')
  (-2, 'exception')
+ (-3, 'line')
+ (-3, 'exception')
- (-2, 'return')
?   ^

+ (-3, 'return')
?   ^

  (1, 'exception')
  (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_break_through_finally (__main__.TraceTestCase.test_break_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 849, in test_break_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (8, 'line')
+ (4, 'line')
  (10, 'line')
+ (4, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
+ (4, 'line')
+ (10, 'line')
  (7, 'line')
- (10, 'line')
?  ^^

+ (2, 'line')
?  ^

  (13, 'line')
  (13, 'return')

======================================================================
FAIL: test_break_to_break (__main__.TraceTestCase.test_break_to_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1029, in test_break_to_break
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
- (5, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_break_to_continue2 (__main__.TraceTestCase.test_break_to_continue2)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1009, in test_break_to_continue2
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (3, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_class_creation_with_decorator (__main__.TraceTestCase.test_class_creation_with_decorator)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1606, in test_class_creation_with_decorator
    self.run_and_compare(func, [
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (6, 'line')
  (1, 'call')
  (2, 'line')
  (4, 'line')
  (4, 'return')
  (7, 'line')
  (8, 'line')
  (7, 'line')
  (1, 'call')
  (2, 'line')
  (4, 'line')
  (4, 'return')
  (10, 'line')
- (6, 'call')
?  ^

+ (10, 'call')
?  ^^

- (6, 'line')
  (11, 'line')
  (11, 'return')
- (7, 'line')
  (2, 'call')
  (3, 'line')
  (3, 'return')
- (6, 'line')
  (2, 'call')
  (3, 'line')
  (3, 'return')
- (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_class_creation_with_docstrings (__main__.TraceTestCase.test_class_creation_with_docstrings)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1582, in test_class_creation_with_docstrings
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (1, 'call')
- (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'return')
  (1, 'return')

======================================================================
FAIL: test_continue_through_finally (__main__.TraceTestCase.test_continue_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 884, in test_continue_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
  (8, 'line')
+ (4, 'line')
  (10, 'line')
+ (4, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (6, 'line')
+ (4, 'line')
+ (10, 'line')
  (7, 'line')
- (10, 'line')
- (3, 'line')
?  ^

+ (2, 'line')
?  ^

  (13, 'line')
  (13, 'return')

======================================================================
FAIL: test_early_exit_with (__main__.TraceTestCase.test_early_exit_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1256, in test_early_exit_with
    self.run_and_compare(func_break,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-5, 'call')
  (-4, 'line')
  (-4, 'return')
- (3, 'line')
- (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'return')
+ (3, 'line')
  (4, 'line')
  (4, 'return')

======================================================================
FAIL: test_finally_with_conditional (__main__.TraceTestCase.test_finally_with_conditional)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 961, in test_finally_with_conditional
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (5, 'line')
  (6, 'line')
+ (2, 'line')
+ (2, 'exception')
  (8, 'line')
  (9, 'line')
  (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_if_break (__main__.TraceTestCase.test_if_break)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 819, in test_if_break
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (2, 'line')
+ (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (8, 'line')
  (8, 'return')

======================================================================
FAIL: test_if_false_in_try_except (__main__.TraceTestCase.test_if_false_in_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1138, in test_if_false_in_try_except
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
- (2, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_if_in_if_in_if (__main__.TraceTestCase.test_if_in_if_in_if)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1232, in test_if_in_if_in_if
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
- (2, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_implicit_return_in_class (__main__.TraceTestCase.test_implicit_return_in_class)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1153, in test_implicit_return_in_class
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (1, 'call')
- (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
- (3, 'return')
?  ^

+ (5, 'return')
?  ^

  (1, 'return')

======================================================================
FAIL: test_loop_in_try_except (__main__.TraceTestCase.test_loop_in_try_except)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 713, in test_loop_in_try_except
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (1, 'line')
+ (3, 'line')
  (3, 'return')

======================================================================
FAIL: test_nested_ifs (__main__.TraceTestCase.test_nested_ifs)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1050, in test_nested_ifs
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (2, 'line')
- (4, 'return')
?  ^

+ (8, 'return')
?  ^


======================================================================
FAIL: test_nested_ifs_with_and (__main__.TraceTestCase.test_nested_ifs_with_and)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1074, in test_nested_ifs_with_and
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
+ (1, 'line')
- (3, 'return')
?  ^

+ (9, 'return')
?  ^


======================================================================
FAIL: test_return_through_finally (__main__.TraceTestCase.test_return_through_finally)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 911, in test_return_through_finally
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (4, 'line')
+ (2, 'line')
- (4, 'return')
?  ^

+ (2, 'return')
?  ^


======================================================================
FAIL: test_tracing_exception_raised_in_with (__main__.TraceTestCase.test_tracing_exception_raised_in_with)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1357, in test_tracing_exception_raised_in_with
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (-5, 'call')
  (-4, 'line')
  (-4, 'return')
  (3, 'line')
  (3, 'exception')
- (2, 'line')
  (-3, 'call')
  (-2, 'line')
  (-2, 'return')
+ (2, 'exception')
  (4, 'line')
  (5, 'line')
- (5, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_no_exception (__main__.TraceTestCase.test_try_except_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 738, in test_try_except_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
  (7, 'line')
  (10, 'line')
  (11, 'line')
+ (1, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_star_exception_caught (__main__.TraceTestCase.test_try_except_star_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1435, in test_try_except_star_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_exception_not_caught (__main__.TraceTestCase.test_try_except_star_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1478, in test_try_except_star_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_caught (__main__.TraceTestCase.test_try_except_star_named_exception_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1457, in test_try_except_star_named_exception_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (2, 'exception')
  (3, 'line')
  (4, 'line')
+ (3, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_exception_not_caught (__main__.TraceTestCase.test_try_except_star_named_exception_not_caught)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1500, in test_try_except_star_named_exception_not_caught
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_named_no_exception (__main__.TraceTestCase.test_try_except_star_named_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1415, in test_try_except_star_named_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
+ (1, 'line')
  (8, 'line')
+ (1, 'line')
- (8, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_except_star_nested (__main__.TraceTestCase.test_try_except_star_nested)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1534, in test_try_except_star_nested
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (4, 'line')
  (5, 'line')
  (3, 'line')
  (3, 'exception')
  (6, 'line')
  (7, 'line')
+ (6, 'line')
  (8, 'line')
  (10, 'line')
  (11, 'line')
+ (11, 'exception')
+ (2, 'line')
+ (2, 'exception')
  (12, 'line')
  (13, 'line')
  (14, 'line')
  (14, 'exception')
  (15, 'line')
  (17, 'line')
  (18, 'line')
+ (17, 'line')
+ (13, 'line')
+ (12, 'line')
+ (1, 'line')
  (19, 'line')
  (19, 'return')

======================================================================
FAIL: test_try_except_star_no_exception (__main__.TraceTestCase.test_try_except_star_no_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1392, in test_try_except_star_no_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
+ (1, 'line')
  (6, 'line')
  (7, 'line')
  (10, 'line')
  (11, 'line')
+ (1, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_except_with_wrong_type (__main__.TraceTestCase.test_try_except_with_wrong_type)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 932, in test_try_except_with_wrong_type
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (7, 'line')
+ (2, 'line')
+ (2, 'exception')
  (8, 'line')
  (9, 'line')
  (10, 'line')
  (10, 'return')

======================================================================
FAIL: test_try_exception_in_else (__main__.TraceTestCase.test_try_exception_in_else)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 767, in test_try_exception_in_else
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
  (7, 'line')
  (8, 'line')
  (8, 'exception')
  (10, 'line')
+ (2, 'line')
+ (2, 'exception')
  (11, 'line')
  (12, 'line')
  (14, 'line')
+ (1, 'line')
- (14, 'return')
?   -

+ (1, 'return')

======================================================================
FAIL: test_try_in_try (__main__.TraceTestCase.test_try_in_try)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1173, in test_try_in_try
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
+ (2, 'line')
+ (1, 'line')
- (3, 'return')
?  ^

+ (1, 'return')
?  ^


======================================================================
FAIL: test_try_in_try_with_exception (__main__.TraceTestCase.test_try_in_try_with_exception)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_sys_settrace.py", line 1191, in test_try_in_try_with_exception
    self.run_and_compare(func,
  File "/tmp/test_sys_settrace.py", line 391, in run_and_compare
    self.compare_events(func.__code__.co_firstlineno,
  File "/tmp/test_sys_settrace.py", line 381, in compare_events
    self.fail(
AssertionError: events did not match expectation:
  (0, 'call')
  (1, 'line')
  (2, 'line')
  (3, 'line')
  (3, 'exception')
  (4, 'line')
+ (2, 'line')
+ (2, 'exception')
  (6, 'line')
  (7, 'line')
- (7, 'return')
?  ^

+ (1, 'return')
?  ^


----------------------------------------------------------------------
Ran 448 tests in Ns

FAILED (failures=158, errors=98, skipped=77)
