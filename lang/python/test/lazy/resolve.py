# The proxy itself: resolve(), its repr, its attributes, and submodules a
# lazy import promised its package.
import sys

lazy import mods.pkg.deep
print(repr(globals()["mods"]))
try:
    globals()["mods"].pkg
except AttributeError as e:
    print("AttributeError:", e)
m = globals()["mods"].resolve()
print(type(m).__name__, m.__name__, "mods.pkg.deep" in sys.modules)
print(type(globals()["mods"]).__name__)
alias = globals()["mods"]
print(type(globals()["alias"]).__name__, type(alias).__name__, type(globals()["alias"]).__name__)
print(mods.pkg.deep.NAME, mods.pkg.deep.value())
import mods.lz_compat
print(sys.get_lazy_imports(), sys.get_lazy_imports_filter())
try:
    sys.set_lazy_imports("sometimes")
except ValueError as e:
    print("ValueError:", e)
import mods.lz_cycle
seen = []


def keep_some(importer, name, fromlist):
    seen.append((importer, name, fromlist))
    return name != "mods.lz_b"


sys.set_lazy_imports_filter(keep_some)
lazy import mods.lz_b
print(type(globals()["mods"]).__name__, seen)
sys.set_lazy_imports_filter(None)
for code in ["def f():\n    lazy import json", "class C:\n    lazy import json",
             "try:\n    lazy import json\nexcept:\n    pass", "lazy from json import *",
             "lazy from __future__ import annotations",
             "try:\n    pass\nfinally:\n    lazy from json import loads"]:
    try:
        compile(code, "s", "exec")
    except SyntaxError as e:
        print(e.msg)
try:
    exec("lazy import json", {}, {})
except SyntaxError as e:
    print("exec:", e)
lazy = 5
lazy += 1
print(lazy)
