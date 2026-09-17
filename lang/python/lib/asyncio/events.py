"""Handles, and the loop a thread is running.

This module is this port's, not CPython's. CPython's names `socket` and
`subprocess` in the default arguments of AbstractEventLoop, and neither is
here, so the abstract class carries only the methods a Braam program can
reach; `AbstractServer` is gone with them. Everything else is the same: a
Handle is a callback with a context, a TimerHandle is one with a time, and
the running loop is a module global.
"""

__all__ = (
    "AbstractEventLoop",
    "Handle",
    "TimerHandle",
    "get_event_loop",
    "set_event_loop",
    "new_event_loop",
    "_set_running_loop",
    "get_running_loop",
    "_get_running_loop",
)

import contextvars
import sys

from . import format_helpers


class Handle:
    """A callback the loop owes, and the context to run it in."""

    __slots__ = (
        "_callback",
        "_args",
        "_cancelled",
        "_loop",
        "_source_traceback",
        "_repr",
        "_context",
        "__weakref__",
    )

    def __init__(self, callback, args, loop, context=None):
        if context is None:
            context = contextvars.copy_context()
        self._context = context
        self._loop = loop
        self._callback = callback
        self._args = args
        self._cancelled = False
        self._repr = None
        if loop.get_debug():
            self._source_traceback = format_helpers.extract_stack(sys._getframe(1))
        else:
            self._source_traceback = None

    def _repr_info(self):
        info = [self.__class__.__name__]
        if self._cancelled:
            info.append("cancelled")
        if self._callback is not None:
            info.append(format_helpers._format_callback_source(self._callback, self._args))
        if self._source_traceback:
            frame = self._source_traceback[-1]
            info.append(f"created at {frame[0]}:{frame[1]}")
        return info

    def __repr__(self):
        if self._repr is not None:
            return self._repr
        info = self._repr_info()
        return "<{}>".format(" ".join(info))

    def cancel(self):
        if not self._cancelled:
            self._cancelled = True
            if self._loop.get_debug():
                self._repr = repr(self)
            self._callback = None
            self._args = None

    def cancelled(self):
        return self._cancelled

    def _run(self):
        try:
            self._context.run(self._callback, *self._args)
        except (SystemExit, KeyboardInterrupt):
            raise
        except BaseException as exc:
            cb = format_helpers._format_callback_source(self._callback, self._args)
            msg = f"Exception in callback {cb}"
            context = {
                "message": msg,
                "exception": exc,
                "handle": self,
            }
            if self._source_traceback:
                context["source_traceback"] = self._source_traceback
            self._loop.call_exception_handler(context)
        self = None


class TimerHandle(Handle):
    """A Handle the loop keeps until `when`."""

    __slots__ = ["_scheduled", "_when"]

    def __init__(self, when, callback, args, loop, context=None):
        super().__init__(callback, args, loop, context)
        if self._source_traceback:
            del self._source_traceback[-1]
        self._when = when
        self._scheduled = False

    def _repr_info(self):
        info = super()._repr_info()
        pos = 2 if self._cancelled else 1
        info.insert(pos, f"when={self._when}")
        return info

    def __hash__(self):
        return hash(self._when)

    def __lt__(self, other):
        if isinstance(other, TimerHandle):
            return self._when < other._when
        return NotImplemented

    def __le__(self, other):
        if isinstance(other, TimerHandle):
            return self._when < other._when or self.__eq__(other)
        return NotImplemented

    def __gt__(self, other):
        if isinstance(other, TimerHandle):
            return self._when > other._when
        return NotImplemented

    def __ge__(self, other):
        if isinstance(other, TimerHandle):
            return self._when > other._when or self.__eq__(other)
        return NotImplemented

    def __eq__(self, other):
        if isinstance(other, TimerHandle):
            return (
                self._when == other._when
                and self._callback == other._callback
                and self._args == other._args
                and self._cancelled == other._cancelled
            )
        return NotImplemented

    def cancel(self):
        if not self._cancelled:
            self._loop._timer_handle_cancelled(self)
        super().cancel()

    def when(self):
        return self._when


class AbstractEventLoop:
    """What a loop must answer. A Braam loop has no sockets and no
    subprocesses, so the methods for those are not here at all."""

    def run_forever(self):
        raise NotImplementedError

    def run_until_complete(self, future):
        raise NotImplementedError

    def stop(self):
        raise NotImplementedError

    def is_running(self):
        raise NotImplementedError

    def is_closed(self):
        raise NotImplementedError

    def close(self):
        raise NotImplementedError

    async def shutdown_asyncgens(self):
        raise NotImplementedError

    async def shutdown_default_executor(self, timeout=None):
        raise NotImplementedError

    def call_soon(self, callback, *args, context=None):
        raise NotImplementedError

    def call_later(self, delay, callback, *args, context=None):
        raise NotImplementedError

    def call_at(self, when, callback, *args, context=None):
        raise NotImplementedError

    def time(self):
        raise NotImplementedError

    def create_future(self):
        raise NotImplementedError

    def create_task(self, coro, **kwargs):
        raise NotImplementedError

    def call_soon_threadsafe(self, callback, *args, context=None):
        raise NotImplementedError

    def run_in_executor(self, executor, func, *args):
        raise NotImplementedError

    def set_default_executor(self, executor):
        raise NotImplementedError

    def get_exception_handler(self):
        raise NotImplementedError

    def set_exception_handler(self, handler):
        raise NotImplementedError

    def default_exception_handler(self, context):
        raise NotImplementedError

    def call_exception_handler(self, context):
        raise NotImplementedError

    def get_task_factory(self):
        raise NotImplementedError

    def set_task_factory(self, factory):
        raise NotImplementedError

    def get_debug(self):
        raise NotImplementedError

    def set_debug(self, enabled):
        raise NotImplementedError


# A Braam process is one Web Worker, so there is one loop and no policy to
# choose it with; `_running` is what get_running_loop answers.
_running_loop = None
_current_loop = None


def _get_running_loop():
    return _running_loop


def _set_running_loop(loop):
    global _running_loop
    _running_loop = loop


def get_running_loop():
    loop = _get_running_loop()
    if loop is None:
        raise RuntimeError("no running event loop")
    return loop


def new_event_loop():
    from . import base_events

    return base_events.BraamEventLoop()


def set_event_loop(loop):
    global _current_loop
    if loop is not None and not isinstance(loop, AbstractEventLoop):
        raise TypeError(f"loop must be an instance of AbstractEventLoop or None, not '{type(loop).__qualname__}'")
    _current_loop = loop


def get_event_loop():
    """The running loop, or the one set for this process."""
    global _current_loop
    running = _get_running_loop()
    if running is not None:
        return running
    if _current_loop is None:
        raise RuntimeError(
            "There is no current event loop in thread 'MainThread'."
        )
    return _current_loop
