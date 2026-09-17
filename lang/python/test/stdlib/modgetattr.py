# from m import x through a module's __getattr__.
import sys, types
m = types.ModuleType("dyn")
def ga(name):
    if name == "x":
        return 42
    raise AttributeError(name)
m.__getattr__ = ga
sys.modules["dyn"] = m
from dyn import x
print(x)
try:
    from dyn import y
except ImportError as e:
    print(type(e).__name__, e)
