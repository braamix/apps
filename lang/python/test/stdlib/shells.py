# The modules that waited for os: shlex, fnmatch, glob, shutil, tempfile,
# pathlib.
import fnmatch
import glob
import os
import pathlib
import shlex
import shutil
import tempfile

print(shlex.split("a 'b c' \"d $e\" f\\ g # h"), shlex.split("x # y", comments=True))
print(shlex.quote("it's"), shlex.quote(""), shlex.quote("safe-1.0"), shlex.join(["a b", "c"]))
lx = shlex.shlex("a=b; c && d", posix=True, punctuation_chars=True)
print(list(lx))
try:
    shlex.split("'open")
except ValueError as e:
    print(e)

print(fnmatch.fnmatch("x.py", "*.py"), fnmatch.fnmatchcase("X.PY", "*.py"), fnmatch.translate("a[!b]?*"))
print(fnmatch.filter(["a.c", "b.h", "c.c"], "*.c"), fnmatch.fnmatch("[x]", "[[]x]"))

top = tempfile.mkdtemp(prefix="sh-")
print(os.path.basename(top).startswith("sh-"), os.path.isdir(top))
os.chdir(top)
for name in ("a.txt", "b.txt", "c.py", "sub/d.txt", "sub/deep/e.txt", ".hidden"):
    os.makedirs(os.path.dirname(name) or ".", exist_ok=True)
    with open(name, "w") as f:
        f.write(name)
print(sorted(glob.glob("*.txt")), sorted(glob.glob("**/*.txt", recursive=True)))
print(sorted(glob.glob("*")), sorted(glob.glob(".*")), glob.glob("nope*"))
print(sorted(glob.iglob("sub/*")), glob.escape("a*[b]?"), sorted(glob.glob("?.py")))
print(sorted(glob.glob("*.txt", root_dir="sub")), glob.has_magic("a[b"))

shutil.copy("a.txt", "a2.txt")
shutil.copyfile("a.txt", "a3.txt")
shutil.copy2("a.txt", "sub")
with open("a2.txt") as f:
    print(f.read(), sorted(os.listdir("sub")))
shutil.copytree("sub", "sub2")
print(sorted(p for p, _, _ in os.walk("sub2")))
shutil.move("a3.txt", "sub2/moved.txt")
print(os.path.exists("a3.txt"), sorted(os.listdir("sub2")))
shutil.rmtree("sub2")
print(os.path.exists("sub2"), shutil.which("definitely-not-here"))
try:
    shutil.copyfile("a.txt", "a.txt")
except shutil.SameFileError as e:
    print(type(e).__name__)
try:
    shutil.rmtree("a.txt")
except NotADirectoryError as e:
    print(type(e).__name__)
print(shutil.get_terminal_size((80, 24)).columns > 0)

with tempfile.TemporaryFile() as t:
    t.write(b"temp")
    t.seek(0)
    print(t.read())
with tempfile.NamedTemporaryFile("w+", suffix=".x", dir=top) as t:
    t.write("named")
    t.flush()
    print(t.name.endswith(".x"), os.path.exists(t.name), t.seek(0), t.read())
    kept = t.name
print(os.path.exists(kept))
with tempfile.TemporaryDirectory(dir=top) as d:
    print(os.path.isdir(d))
print(os.path.isdir(d))
fd, name = tempfile.mkstemp(text=True, dir=top)
os.write(fd, b"mk")
os.close(fd)
with open(name) as f:
    print(f.read(), tempfile.gettempdir() == tempfile.gettempdir())
os.remove(name)
with tempfile.SpooledTemporaryFile(max_size=4) as s:
    s.write(b"ab")
    print(s._rolled)
    s.write(b"cdef")
    print(s._rolled, s.tell())

P = pathlib.PurePosixPath
p = P("/usr/lib/python3.tar.gz")
print(p.name, p.stem, p.suffix, p.suffixes, p.parent, p.parts, p.anchor)
print(p.with_suffix(".zip"), p.with_name("x"), p.with_stem("y"), p.relative_to("/usr"))
print(P("a") / "b" / "../c", P("a/b").joinpath("c", "d"), list(P("/a/b/c").parents))
print(P("a/b.py").match("*.py"), P("/a") == P("//a"), P("a") < P("b"), P("x").is_absolute())
print(P("a/b").full_match("a/*"), repr(P("q")), str(P("")), P("a/b").as_posix())
try:
    P("/a").relative_to("/b")
except ValueError as e:
    print(e)
q = pathlib.Path(top)
print(type(q).__name__, q.exists(), q.is_dir(), (q / "a.txt").is_file())
print((q / "a.txt").read_text(), (q / "b.txt").read_bytes())
(q / "new.txt").write_text("hello\n")
print((q / "new.txt").stat().st_size, sorted(x.name for x in q.iterdir()))
print(sorted(str(x.relative_to(q)) for x in q.glob("*.txt")))
print(sorted(str(x.relative_to(q)) for x in q.rglob("*.txt")))
(q / "made" / "deeper").mkdir(parents=True)
(q / "made" / "deeper").rmdir()
(q / "new.txt").rename(q / "renamed.txt")
print((q / "renamed.txt").exists(), (q / "new.txt").exists())
(q / "renamed.txt").unlink()
(q / "renamed.txt").unlink(missing_ok=True)
with (q / "a.txt").open() as f:
    print(f.read())
(q / "t").touch()
print((q / "t").stat().st_size, pathlib.Path("a/../b").resolve() == pathlib.Path.cwd() / "b")
print([str(r.relative_to(q)) for r, ds, fs in q.walk() if "deep" in str(r)])
print(pathlib.Path("~").expanduser() == pathlib.Path.home(), pathlib.Path(".").absolute() == q)
os.chdir("/")
shutil.rmtree(top)
print(os.path.exists(top))
