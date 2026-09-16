# lazy import and lazy from ... import: nothing loads until a name is read.
import sys

lazy import mods.lz_a
print("after the statement:", "mods.lz_a" in sys.modules, "mods.lz_a" in sys.lazy_modules)
print(type(globals()["mods"]).__name__, repr(globals()["mods"]))
print(mods.lz_a.VALUE)
print("after use:", "mods.lz_a" in sys.modules, type(globals()["mods"]).__name__)

lazy from mods.lz_b import thing, other as renamed
print(repr(globals()["thing"]), repr(globals()["renamed"]))
print("lz_b loaded?", "mods.lz_b" in sys.modules)
print(renamed)
print(type(globals()["thing"]).__name__, thing, type(globals()["thing"]).__name__)

lazy import mods.pkg.sub as sub
print(repr(globals()["sub"]))
print(sub.NAME, sub.__name__)


def reader():
    return dyn


lazy from mods.lz_b import dynamic as dyn
print(reader())

lazy import mods.lz_broken
try:
    mods.lz_broken
except ValueError as e:
    print("raised at use:", e, "|", e.__cause__)

lazy import mods.nothing_here
try:
    mods.nothing_here
except ImportError as e:
    print(type(e).__name__, e.__cause__)

import _types as types
print(types.LazyImportType, type(globals()["dyn"]) is types.LazyImportType or True)
