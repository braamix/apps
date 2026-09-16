__lazy_modules__ = ["mods.lz_a"]
import mods.lz_a
import sys
print("compat: lz_a loaded?", "mods.lz_a" in sys.modules, type(globals()["mods"]).__name__)
