# sys.monitoring: the registry, and the events that fire.
#
# CPython's own test_monitoring has no main block -- regrtest discovers it --
# so it cannot be driven the way the other cases are; this is what says the
# events arrive in the order and with the arguments PEP 669 states.
import sys

M = sys.monitoring
E = M.events

print(M.DEBUGGER_ID, M.COVERAGE_ID, M.PROFILER_ID, M.OPTIMIZER_ID)
print(E.PY_START, E.PY_RESUME, E.PY_RETURN, E.PY_YIELD, E.CALL, E.LINE)
print(E.INSTRUCTION, E.JUMP, E.RAISE, E.PY_UNWIND, E.NO_EVENTS)

# The registry: an id is taken, named, and given back.
print(M.get_tool(M.DEBUGGER_ID))
M.use_tool_id(M.DEBUGGER_ID, "mine")
print(M.get_tool(M.DEBUGGER_ID))
try:
    M.use_tool_id(M.DEBUGGER_ID, "other")
except ValueError as e:
    print("in use:", e)
try:
    M.use_tool_id(6, "sys")
except ValueError as e:
    print("reserved:", e)
try:
    M.get_events(M.COVERAGE_ID)
except ValueError as e:
    print("not in use:", e)

# BRANCH is the two halves, and C_RETURN may not be asked for on its own.
M.set_events(M.DEBUGGER_ID, E.BRANCH)
print(M.get_events(M.DEBUGGER_ID) == E.BRANCH_LEFT | E.BRANCH_RIGHT)
try:
    M.set_events(M.DEBUGGER_ID, E.C_RETURN)
except ValueError as e:
    print("c_return:", e)
M.set_events(M.DEBUGGER_ID, 0)


def target(n):
    total = 0
    for i in range(n):
        total += i
    return total


def failing():
    raise ValueError("no")


def catching():
    try:
        failing()
    except ValueError:
        return "caught"


def counting():
    yield 1
    yield 2


# A callback for each event, recording what it was handed. An instruction
# offset is this interpreter's own, so only a line number and an event's
# value are printed.
log = []


def watcher(name, kind):
    def cb(code, *rest):
        if kind == "line":
            log.append((name, code.co_name, rest[0]))
        elif kind == "value":
            v = rest[1]
            log.append((name, code.co_name, v if isinstance(v, int) else type(v).__name__))
        else:
            log.append((name, code.co_name))

    return cb


for event, name, kind in [
    (E.PY_START, "start", ""),
    (E.PY_RESUME, "resume", ""),
    (E.PY_RETURN, "return", "value"),
    (E.PY_YIELD, "yield", "value"),
    (E.LINE, "line", "line"),
    (E.JUMP, "jump", ""),
    (E.RAISE, "raise", "value"),
    (E.PY_UNWIND, "unwind", "value"),
]:
    print(M.register_callback(M.DEBUGGER_ID, event, watcher(name, kind)))

# The callback that was there comes back, and None takes it off.
again = watcher("start", "")
old = M.register_callback(M.DEBUGGER_ID, E.PY_START, again)
print(old is not None, M.register_callback(M.DEBUGGER_ID, E.PY_START, old) is again)

M.set_events(M.DEBUGGER_ID, E.PY_START | E.PY_RESUME | E.PY_RETURN | E.PY_YIELD |
             E.RAISE | E.PY_UNWIND)
target(3)
catching()
list(counting())
M.set_events(M.DEBUGGER_ID, 0)
for row in log:
    print(row)

# Local events: only the one code object reports lines.
log.clear()
M.set_local_events(M.DEBUGGER_ID, target.__code__, E.LINE | E.PY_RETURN)
print(M.get_local_events(M.DEBUGGER_ID, target.__code__) == E.LINE | E.PY_RETURN)
print(M.get_local_events(M.DEBUGGER_ID, catching.__code__))
target(2)
catching()
for row in log:
    print(row)

# _all_events reports the global set by name, and nothing of the local one.
M.set_events(M.DEBUGGER_ID, E.RAISE)
print(M._all_events())
M.set_events(M.DEBUGGER_ID, 0)

M.clear_tool_id(M.DEBUGGER_ID)
print(M.get_tool(M.DEBUGGER_ID), M.get_events(M.DEBUGGER_ID),
      M.get_local_events(M.DEBUGGER_ID, target.__code__))
M.free_tool_id(M.DEBUGGER_ID)
print(M.get_tool(M.DEBUGGER_ID))
