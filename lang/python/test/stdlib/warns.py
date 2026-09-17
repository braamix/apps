# warnings over the native _warnings: the default filters, the actions, and
# catch_warnings recording what was said.
import warnings
# The golden's interpreter runs under -W ignore::DeprecationWarning, which is
# a filter in front of these.
print(warnings.filters[-3:])
print(warnings.defaultaction, warnings.onceregistry, type(warnings._lock).__name__)


def said(*args, **kw):
    with warnings.catch_warnings(record=True) as log:
        warnings.simplefilter("always")
        warnings.warn(*args, **kw)
    return [(str(w.message), w.category.__name__, w.lineno) for w in log]


print(said("plain"))
print(said("dep", DeprecationWarning))
print(said(UserWarning("an instance")))


def inner():
    return said("up", stacklevel=2)


print(inner())
with warnings.catch_warnings(record=True) as log:
    warnings.simplefilter("once")
    for i in range(3):
        warnings.warn("once only", RuntimeWarning)
    warnings.simplefilter("ignore", SyntaxWarning)
    warnings.warn("hidden", SyntaxWarning)
print(len(log), [str(w.message) for w in log])
with warnings.catch_warnings():
    warnings.simplefilter("error", UserWarning)
    try:
        warnings.warn("boom")
    except UserWarning as e:
        print("raised", e)
print(warnings.filters[-3:])
try:
    warnings.simplefilter("nonsense")
except ValueError as e:
    print(e)
try:
    warnings.warn("x", category=int)
except TypeError as e:
    print(e)

