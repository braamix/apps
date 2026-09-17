# importlib over the loader: specs, finders the program adds, reloading.
import importlib
import importlib.machinery
import importlib.util
import os
import sys
import tempfile

import json
import errno
import textwrap

print(json.__spec__.name, json.__spec__.parent, json.__package__)
print(json.__spec__.submodule_search_locations == json.__path__)
print(type(json.__loader__).__name__, json.__loader__ is json.__spec__.loader)
print(os.path.basename(json.__spec__.origin), json.__spec__.has_location)
print(errno.__spec__.origin, errno.__loader__ is importlib.machinery.BuiltinImporter)
print(textwrap.__spec__.parent == "", textwrap.__spec__.name)
print([f.__name__ for f in sys.meta_path])
print(sys.path_hooks[-1].__name__, sys.modules["__main__"].__spec__)

m = importlib.import_module("fnmatch")
print(m.__name__, m.__spec__.origin == m.__file__)
print(importlib.import_module("..decoder", "json.scanner").__name__)
print(importlib.import_module(".encoder", "json").__name__)
print(importlib.util.resolve_name(".x", "pkg"), importlib.util.resolve_name("..y", "pkg.sub"))
print(importlib.util.find_spec("glob").name, importlib.util.find_spec("no_such_module_here"))
print(importlib.util.find_spec("json.decoder").parent)
try:
    importlib.import_module("no_such_module_here")
except ModuleNotFoundError as e:
    print(type(e).__name__, e, e.name, e.path)
try:
    importlib.util.resolve_name("..x", "top")
except ImportError as e:
    print(e)
e = ImportError("m", name="n", path="p")
print(e.msg, e.name, e.path, e.name_from, str(e), e.args)
e.name = "other"
print(e.name, repr(ModuleNotFoundError("gone", name="x")))
try:
    ImportError("m", bogus=1)
except TypeError as e:
    print(e)

d = tempfile.mkdtemp()
sys.path.insert(0, d)
with open(os.path.join(d, "reloaded.py"), "w") as f:
    f.write("VALUE = 1\n")
os.makedirs(os.path.join(d, "apkg", "inner"))
with open(os.path.join(d, "apkg", "__init__.py"), "w") as f:
    f.write("from . import leaf\nWHO = __name__\n")
with open(os.path.join(d, "apkg", "leaf.py"), "w") as f:
    f.write("import apkg as up\nNAME = __spec__.name, __package__, hasattr(up, 'WHO')\n")
import reloaded
import apkg
import apkg.inner
print(reloaded.VALUE, apkg.WHO, apkg.leaf.NAME,
      apkg.__spec__.submodule_search_locations == [os.path.join(d, "apkg")])
print(apkg.inner.__spec__.origin, apkg.inner.__spec__.loader is not None,
      list(apkg.inner.__path__) == [os.path.join(d, "apkg", "inner")])
with open(os.path.join(d, "reloaded.py"), "w") as f:
    f.write("VALUE = 2\nEXTRA = 'yes'\n")
print(importlib.reload(reloaded) is reloaded, reloaded.VALUE, reloaded.EXTRA)
print(importlib.reload(errno) is errno)
try:
    importlib.reload(42)
except TypeError as e:
    print(e)


class Loader:
    def create_module(self, spec):
        return None

    def exec_module(self, module):
        module.where = "virtual " + module.__name__
        if module.__spec__.submodule_search_locations is not None:
            module.__path__ = []


class Finder:
    def find_spec(self, name, path, target=None):
        if name.split(".")[0] == "virt":
            return importlib.util.spec_from_loader(name, Loader(), is_package=(name == "virt"))
        return None


finder = Finder()
sys.meta_path.insert(0, finder)
import virt
import virt.sub
from virt import other
print(virt.where, virt.sub.where, other.where, virt.sub.__package__, virt.other is other)
print(virt.__spec__.loader.__class__.__name__, virt.__path__)
import encodings.cp1252 as tool
print(tool.__name__)
try:
    import definitely_not_here
except ModuleNotFoundError as e:
    print(e)
sys.meta_path.remove(finder)


class HookFinder:
    def find_spec(self, name, target=None):
        if name == "hooked_mod":
            return importlib.util.spec_from_loader(name, Loader())
        return None


def hook(path):
    if path == "<hooked>":
        return HookFinder()
    raise ImportError


sys.path_hooks.insert(0, hook)
sys.path.append("<hooked>")
import hooked_mod
print(hooked_mod.where, type(sys.path_importer_cache["<hooked>"]).__name__)

spec = importlib.util.spec_from_loader("dyn", loader=None)
dyn = importlib.util.module_from_spec(spec)
exec("X = 42\n", dyn.__dict__)
print(dyn.X, dyn.__spec__, dyn.__name__)
spec = importlib.util.spec_from_file_location("byfile", os.path.join(d, "reloaded.py"))
byfile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(byfile)
print(byfile.VALUE, byfile.__name__, "byfile" in sys.modules)
print(importlib.machinery.SOURCE_SUFFIXES, importlib.util.MAGIC_NUMBER[2:])
print(importlib.util.decode_source(b"# coding: latin-1\nx = '\xe9'\n"))
print(importlib.__import__("json").__name__, importlib.__import__("json.decoder").__name__)
