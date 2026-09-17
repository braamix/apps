"""The event loop, written for Braam.

This module is this port's, not CPython's. CPython's BaseEventLoop is built
round a selector: it waits in `select()` and wakes on a socket. There are no
sockets here, so what the loop waits on is the process itself -- `time.sleep`
parks it through the driver, which is Braam's own event loop, and the kernel
brings it back. The rest is CPython's shape: a deque of ready handles, a heap
of timers, and a run_forever that empties one and then the other.

A callback is not run from inside `run_once`'s scan of the ready queue, as
CPython's is not either: the queue is taken whole, so a callback that adds
another one is run on the next turn and cannot starve the timers.

The clock may not move. Under the headless test harness `proc_now()` is
frozen on purpose, so a park returns with `time.monotonic()` exactly where it
was and a timer would never come due. The loop therefore counts what it
slept: where a park did not advance the clock, the time it asked for is added
to an offset of its own. In a browser the clock does move and the offset
stays at zero.
"""

__all__ = ("BaseEventLoop", "BraamEventLoop")

import collections
import heapq
import inspect
import sys
import time
import traceback
import weakref

from . import coroutines
from . import events
from . import futures
from . import tasks
from .log import logger

# When the heap is this full and this much of it is cancelled, sweep it.
_MIN_SCHEDULED_TIMER_HANDLES = 100
_MIN_CANCELLED_TIMER_HANDLES_FRACTION = 0.5

# The longest one park may be, as CPython's selector loop caps its select().
MAXIMUM_SLEEP = 24 * 3600


class BaseEventLoop(events.AbstractEventLoop):
    def __init__(self):
        self._closed = False
        self._stopping = False
        self._ready = collections.deque()
        self._scheduled = []
        self._timer_cancelled_count = 0
        self._debug = False
        self._thread_id = None
        self._exception_handler = None
        self._task_factory = None
        self._asyncgens = weakref.WeakSet()
        self._asyncgens_shutdown_called = False
        self._executor_shutdown_called = False
        self._default_executor = None
        self._current_handle = None
        self._coroutine_origin_tracking_enabled = False
        self._clock_resolution = 1e-3
        self._offset = 0.0
        # A callback slower than this is logged in debug mode, as CPython's.
        self.slow_callback_duration = 0.1

    def __repr__(self):
        return (
            f"<{self.__class__.__name__} running={self.is_running()} "
            f"closed={self.is_closed()} debug={self.get_debug()}>"
        )

    # --------------------------------------------------------------- state

    def is_running(self):
        return self._thread_id is not None

    def is_closed(self):
        return self._closed

    def close(self):
        if self.is_running():
            raise RuntimeError("Cannot close a running event loop")
        if self._closed:
            return
        self._closed = True
        self._ready.clear()
        self._scheduled.clear()
        self._executor_shutdown_called = True

    def _check_closed(self):
        if self._closed:
            raise RuntimeError("Event loop is closed")

    def _check_running(self):
        if self.is_running():
            raise RuntimeError("This event loop is already running")
        if events._get_running_loop() is not None:
            raise RuntimeError("Cannot run the event loop while another loop is running")

    def get_debug(self):
        return self._debug

    def set_debug(self, enabled):
        self._debug = bool(enabled)

    def time(self):
        return time.monotonic() + self._offset

    # ---------------------------------------------------------- scheduling

    def call_soon(self, callback, *args, context=None):
        self._check_closed()
        self._check_callback(callback, "call_soon")
        handle = events.Handle(callback, args, self, context)
        self._ready.append(handle)
        return handle

    def call_soon_threadsafe(self, callback, *args, context=None):
        # One Web Worker, so there is no other thread to be safe from.
        return self.call_soon(callback, *args, context=context)

    def call_later(self, delay, callback, *args, context=None):
        if delay is None:
            raise TypeError("delay must not be None")
        return self.call_at(self.time() + delay, callback, *args, context=context)

    def call_at(self, when, callback, *args, context=None):
        if when is None:
            raise TypeError("when cannot be None")
        self._check_closed()
        self._check_callback(callback, "call_at")
        timer = events.TimerHandle(when, callback, args, self, context)
        heapq.heappush(self._scheduled, timer)
        timer._scheduled = True
        return timer

    def _check_callback(self, callback, method):
        if coroutines.iscoroutine(callback) or inspect.iscoroutinefunction(callback):
            raise TypeError(f"coroutines cannot be used with {method}()")
        if not callable(callback):
            raise TypeError(f"a callable object was expected by {method}(): {callback!r}")

    def _timer_handle_cancelled(self, handle):
        if handle._scheduled:
            self._timer_cancelled_count += 1

    # ------------------------------------------------------------- futures

    def create_future(self):
        return futures.Future(loop=self)

    def create_task(self, coro, *, name=None, context=None, **kwargs):
        self._check_closed()
        if self._task_factory is not None:
            task = self._task_factory(self, coro, context=context, **kwargs)
            tasks._set_task_name(task, name)
            return task
        return tasks.Task(coro, loop=self, name=name, context=context, **kwargs)

    def set_task_factory(self, factory):
        if factory is not None and not callable(factory):
            raise TypeError("task factory must be a callable or None")
        self._task_factory = factory

    def get_task_factory(self):
        return self._task_factory

    # -------------------------------------------------------------- errors

    def get_exception_handler(self):
        return self._exception_handler

    def set_exception_handler(self, handler):
        if handler is not None and not callable(handler):
            raise TypeError(f"A callable object or None is expected, got {handler!r}")
        self._exception_handler = handler

    def default_exception_handler(self, context):
        message = context.get("message")
        if not message:
            message = "Unhandled exception in event loop"
        exception = context.get("exception")
        if exception is not None:
            exc_info = (type(exception), exception, exception.__traceback__)
        else:
            exc_info = False

        log_lines = [message]
        for key in sorted(context):
            if key in {"message", "exception"}:
                continue
            value = context[key]
            if key == "source_traceback":
                tb = "".join(traceback.format_list(value))
                value = "Object created at (most recent call last):\n"
                value += tb.rstrip()
            else:
                value = repr(value)
            log_lines.append(f"{key}: {value}")
        logger.error("\n".join(log_lines), exc_info=exc_info)

    def call_exception_handler(self, context):
        if self._exception_handler is None:
            try:
                self.default_exception_handler(context)
            except (SystemExit, KeyboardInterrupt):
                raise
            except BaseException:
                logger.error("Exception in default exception handler", exc_info=True)
            return
        try:
            self._exception_handler(self, context)
        except (SystemExit, KeyboardInterrupt):
            raise
        except BaseException as exc:
            try:
                self.default_exception_handler(
                    {
                        "message": "Unhandled error in exception handler",
                        "exception": exc,
                        "context": context,
                    }
                )
            except (SystemExit, KeyboardInterrupt):
                raise
            except BaseException:
                logger.error("Exception in default exception handler", exc_info=True)

    # ---------------------------------------------------- async generators

    def _asyncgen_firstiter_hook(self, agen):
        if self._asyncgens_shutdown_called:
            import warnings

            warnings.warn(
                f"asynchronous generator {agen!r} was scheduled after "
                f"loop.shutdown_asyncgens() call",
                ResourceWarning,
                source=self,
            )
        self._asyncgens.add(agen)

    def _asyncgen_finalizer_hook(self, agen):
        self._asyncgens.discard(agen)
        if not self.is_closed():
            self.call_soon_threadsafe(self.create_task, agen.aclose())

    async def shutdown_asyncgens(self):
        """Close every asynchronous generator this loop started."""
        self._asyncgens_shutdown_called = True
        if self._asyncgens is None or not len(self._asyncgens):
            return
        closing = list(self._asyncgens)
        self._asyncgens.clear()
        results = await tasks.gather(*[ag.aclose() for ag in closing], return_exceptions=True)
        for result, agen in zip(results, closing):
            if isinstance(result, BaseException):
                self.call_exception_handler(
                    {
                        "message": f"an error occurred during closing of asynchronous generator {agen!r}",
                        "exception": result,
                        "asyncgen": agen,
                    }
                )

    async def shutdown_default_executor(self, timeout=None):
        self._executor_shutdown_called = True

    def set_default_executor(self, executor):
        raise NotImplementedError("there is no executor here: a process is one Web Worker")

    def run_in_executor(self, executor, func, *args):
        raise NotImplementedError("there is no executor here: a process is one Web Worker")

    # --------------------------------------------------------- running it

    def run_forever(self):
        self._check_closed()
        self._check_running()
        self._thread_id = 1
        old_agen_hooks = sys.get_asyncgen_hooks()
        events._set_running_loop(self)
        try:
            sys.set_asyncgen_hooks(
                firstiter=self._asyncgen_firstiter_hook,
                finalizer=self._asyncgen_finalizer_hook,
            )
            while True:
                self._run_once()
                if self._stopping:
                    break
        finally:
            self._stopping = False
            self._thread_id = None
            events._set_running_loop(None)
            sys.set_asyncgen_hooks(*old_agen_hooks)

    def run_until_complete(self, future):
        self._check_closed()
        self._check_running()

        new_task = not futures.isfuture(future)
        future = tasks.ensure_future(future, loop=self)
        if new_task:
            future._log_destroy_pending = False

        future.add_done_callback(_run_until_complete_cb)
        try:
            self.run_forever()
        except BaseException:
            if new_task and future.done() and not future.cancelled():
                future.exception()
            raise
        finally:
            future.remove_done_callback(_run_until_complete_cb)
        if not future.done():
            raise RuntimeError("Event loop stopped before Future completed.")
        return future.result()

    def stop(self):
        self._stopping = True

    def _run_once(self):
        """One turn: the timers that are due, then the ready queue."""
        # A heap full of cancelled timers is swept rather than popped one at
        # a time, which is what CPython does for the same reason.
        sched_count = len(self._scheduled)
        if (
            sched_count > _MIN_SCHEDULED_TIMER_HANDLES
            and self._timer_cancelled_count / sched_count
            > _MIN_CANCELLED_TIMER_HANDLES_FRACTION
        ):
            new_scheduled = []
            for handle in self._scheduled:
                if handle._cancelled:
                    handle._scheduled = False
                else:
                    new_scheduled.append(handle)
            heapq.heapify(new_scheduled)
            self._scheduled = new_scheduled
            self._timer_cancelled_count = 0
        else:
            while self._scheduled and self._scheduled[0]._cancelled:
                self._timer_cancelled_count -= 1
                handle = heapq.heappop(self._scheduled)
                handle._scheduled = False

        # Nothing to run now: park until the next timer is due. With neither
        # a callback nor a timer the loop can never go on, and saying so is
        # better than sleeping for ever.
        if not self._ready and not self._stopping:
            if self._scheduled:
                timeout = self._scheduled[0]._when - self.time()
                if timeout > 0:
                    timeout = min(timeout, MAXIMUM_SLEEP)
                    before = time.monotonic()
                    time.sleep(timeout)
                    if time.monotonic() <= before:
                        self._offset += timeout
            else:
                raise RuntimeError("Event loop has nothing left to do and nothing to wait for")

        end_time = self.time() + self._clock_resolution
        while self._scheduled:
            handle = self._scheduled[0]
            if handle._when >= end_time:
                break
            handle = heapq.heappop(self._scheduled)
            handle._scheduled = False
            self._ready.append(handle)

        # The queue is taken whole, so a callback that adds another one waits
        # for the next turn and the timers are not starved.
        ntodo = len(self._ready)
        for _ in range(ntodo):
            handle = self._ready.popleft()
            if handle._cancelled:
                continue
            if self._debug:
                try:
                    self._current_handle = handle
                    t0 = self.time()
                    handle._run()
                    dt = self.time() - t0
                    if dt >= self.slow_callback_duration:
                        logger.warning("Executing %s took %.3f seconds", handle, dt)
                finally:
                    self._current_handle = None
            else:
                handle._run()
        handle = None


def _run_until_complete_cb(fut):
    if not fut.cancelled():
        exc = fut.exception()
        if isinstance(exc, (SystemExit, KeyboardInterrupt)):
            # The caller is meant to see these, not the loop.
            return
    fut.get_loop().stop()


class BraamEventLoop(BaseEventLoop):
    """The loop a Braam process runs. There is only the one kind."""
