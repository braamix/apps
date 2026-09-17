# os.path over posixpath, and os's own calls over the store.
import os
import os.path
import stat
import tempfile
import warnings

j = os.path.join
print(j("a", "b", "c"), j("/a", "/b"), j("a/", "b"), j("", "x"), j(b"a", b"b"))
print(os.path.split("/a/b/c.txt"), os.path.split("c"), os.path.split("/"))
print(os.path.splitext("x/y.tar.gz"), os.path.splitext(".bashrc"), os.path.splitext("a."))
print(os.path.basename("/a/b/"), os.path.dirname("/a/b/"), os.path.dirname("b"))
print(os.path.normpath("a//b/./c/../d"), os.path.normpath("/../x"), os.path.normpath(""))
print(os.path.normpath("//a"), os.path.normpath("///a"), os.path.normpath(b"a/../../b"))
with warnings.catch_warnings():
    warnings.simplefilter("ignore")
    print(os.path.isabs("/x"), os.path.isabs("x"), os.path.commonprefix(["abc", "abd", "ab"]))
print(os.path.commonpath(["/a/b/c", "/a/b/d", "/a/bb"]), os.path.relpath("/a/b/c", "/a/d"))
print(os.path.splitroot("//x/y"), os.path.splitroot("/x"), os.path.splitroot("x"))
print(os.path.expanduser("~nobodyhere/x"), os.path.expandvars("$NOPE_NOT_SET/${NOPE2}"))
os.environ["BRAAM_T"] = "val"
print(os.path.expandvars("$BRAAM_T/${BRAAM_T}x"), os.environ.get("BRAAM_T"), os.getenv("BRAAM_T"))
del os.environ["BRAAM_T"]
print("BRAAM_T" in os.environ, os.getenv("BRAAM_T", "gone"))
print(os.fspath("p"), os.fsencode("é"), os.fsdecode(b"\xc3\xa9"), ascii(os.fsdecode(b"\xff")))
try:
    os.path.commonpath(["/a", "b"])
except ValueError as e:
    print(e)
try:
    os.path.join("a", 5)
except TypeError as e:
    print(e)
print(os.sep, os.pathsep, os.curdir, os.pardir, os.extsep, os.altsep, os.linesep == "\n")
print(os.name, os.devnull, os.path is __import__("posixpath"))

top = tempfile.mkdtemp()
os.chdir(top)
print(os.getcwd() == top, os.path.realpath(".") == top)
os.makedirs("d1/d2/d3")
os.makedirs("d1/d2", exist_ok=True)
try:
    os.makedirs("d1/d2")
except FileExistsError as e:
    print("exists:", e.filename)
os.mkdir("e")
with open("d1/f.txt", "w") as f:
    f.write("12345")
print(sorted(os.listdir(".")), sorted(os.listdir("d1")))
st = os.stat("d1/f.txt")
print(st.st_size, stat.S_ISREG(st.st_mode), stat.S_ISDIR(os.stat("d1").st_mode))
print(os.path.getsize("d1/f.txt"), os.path.isfile("d1/f.txt"), os.path.isdir("d1"))
print(os.path.exists("nope"), os.path.lexists("nope"), os.path.islink("d1"))
print(type(os.path.getmtime("d1/f.txt")).__name__, os.stat_result is type(st))
print(len(st), st[6], type(st[stat.ST_MTIME]).__name__, type(st.st_mtime).__name__)
os.symlink("d1/f.txt", "ln")
print(os.path.islink("ln"), os.readlink("ln"), os.path.realpath("ln") == os.path.join(top, "d1/f.txt"))
print(os.lstat("ln").st_size != 5 or "same", stat.S_ISLNK(os.lstat("ln").st_mode))
os.rename("d1/f.txt", "e/g.txt")
print(os.path.exists("ln"), os.path.lexists("ln"))
os.replace("e/g.txt", "e/h.txt")
print(os.listdir("e"))
with os.scandir("d1") as it:
    ents = sorted(it, key=lambda e: e.name)
print([(e.name, e.is_dir(), e.is_file(), e.path) for e in ents])
print([(p, sorted(ds), sorted(fs)) for p, ds, fs in os.walk(".")])
print([p for p, ds, fs in os.walk(".", topdown=False)])
for bad in (lambda: os.stat("nope"), lambda: os.rmdir("d1"), lambda: os.mkdir("e"),
            lambda: os.listdir("e/h.txt")):
    try:
        bad()
    except OSError as e:
        print(type(e).__name__, e.filename)
os.unlink("ln")
os.remove("e/h.txt")
os.removedirs("d1/d2/d3")
os.rmdir("e")
print(os.listdir("."))
os.chdir("/")
os.rmdir(top)

fd = os.open(os.path.join(tempfile.gettempdir(), "fdtest"), os.O_RDWR | os.O_CREAT | os.O_TRUNC)
print(os.write(fd, b"hello world"), os.lseek(fd, 0, os.SEEK_CUR), os.lseek(fd, 6, os.SEEK_SET))
print(os.read(fd, 100), os.fstat(fd).st_size, os.isatty(fd))
os.ftruncate(fd, 5)
os.lseek(fd, 0, 0)
print(os.read(fd, 100))
fd2 = os.dup(fd)
print(fd2 != fd, os.get_inheritable(fd2))
os.close(fd2)
os.close(fd)
try:
    os.close(fd)
except OSError as e:
    print("close:", e.errno == 9)
os.unlink(os.path.join(tempfile.gettempdir(), "fdtest"))
r, w = os.pipe()
print(os.write(w, b"piped"), os.read(r, 5))
os.close(r)
os.close(w)
print(type(os.getpid()).__name__, type(os.urandom(4)).__name__, len(os.urandom(4)))
print(os.cpu_count() >= 1, os.get_terminal_size.__name__, type(os.times()).__name__)
print(os.strerror(2), stat.filemode(0o100644), stat.filemode(0o40755))
