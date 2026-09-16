# compile(), eval(), exec() and the four ways a program reads its own
# namespaces. Each of these runs Python from inside a builtin, which is the
# continuation rule again: see README.

x = 1


# eval over source and over a code object.
print(eval("1 + 2"))
print(eval("1 + 2\n"))
print(eval("1 + 2\n\n# a comment\n"))
print(eval("x"))
print(eval("lambda v: v + 10")(-5))
print(eval(compile("x * 100", "<c>", "eval")))
print(eval(b"x + 1"))

# eval sees the namespaces it is given.
print(eval("x", {"x": 7}))
print(eval("x", None, {"x": 8}))
print(eval("x + y", {"x": 10}, {"y": 20}))


# exec defines into the namespace it runs in.
exec("defined_here = 5")
print(defined_here)

d = {}
exec("def f(): return 42", d)
print(d["f"](), "f" in d, "__builtins__" in d)

e = {}
exec("y = x + 1", {"x": 100}, e)
print(e["y"])

# The three modes.
c = compile("print(x)", "<c>", "exec")
exec(c)
exec(c, {"x": 2})
exec(c, {}, {"x": 3})
print(eval(compile("2 ** 8", "<c>", "eval")))
exec(compile("if 1: 10 + 1\n", "<c>", "single"))
exec(compile("None\n", "<c>", "single"))
exec(compile("'text'\n", "<c>", "single"))
exec(compile("print(12)", "<c>", "single"))

# A code object is a value like any other.
c = compile("pass", "<c>", "exec")
print(type(c).__name__, type(hash(c)) is int)

# What a bad call says.
try:
    compile("1", "<c>", "nope")
except ValueError:
    print("ValueError")
try:
    eval("[1,,]")
except SyntaxError:
    print("SyntaxError")
try:
    exec("print(1)", "not a dict")
except TypeError:
    print("TypeError globals")
try:
    exec("print(1)", None, 123)
except TypeError:
    print("TypeError locals")
try:
    exec(compile("nosuchname", "<c>", "exec"))
except NameError:
    print("NameError")
print(x)


# globals(), locals() and vars().
print(globals() is globals(), "x" in globals(), globals()["x"])
print(type(locals()).__name__)


def scope():
    a = 1
    b = "two"
    got = locals()
    return sorted(got), got["a"], got["b"]


print(scope())


def closed():
    inner_free = 9

    def peek():
        return sorted(locals()), inner_free

    return peek()


print(closed())


class Body:
    p = 1

    def q(self):
        pass

    print("p" in locals(), "q" in locals(), locals()["p"])


print(vars() is globals())


class Holder:
    pass


h = Holder()
h.n = 3
print(vars(h))
try:
    vars(1)
except TypeError:
    print("TypeError vars")


# dir()
print("x" in dir(), "__name__" in dir(), "__builtins__" in dir())
print(dir(Holder()) == sorted(dir(Holder())))
print("n" in dir(h))
print("append" in dir(list), "join" in dir(str), "keys" in dir(dict))
print("append" in dir([]), "upper" in dir(""))


class A:
    def a(self):
        pass


class B(A):
    def b(self):
        pass


class C(A):
    def c(self):
        pass


class D(B, C):
    def d(self):
        pass


names = dir(D())
print(names.count("a"), names.count("b"), names.count("c"), names.count("d"))
import sys

print("path" in dir(sys), "modules" in dir(sys))


# An expression may be written with space in front of it; a statement may not.
print(eval("  1 + 2  "), eval("\t7"))
try:
    exec("  pass")
except IndentationError:
    print("IndentationError")
try:
    exec("if 1:\npass")
except IndentationError:
    print("IndentationError block")

# A code object runs whatever mode it was made in.
print(eval(compile("9", "<c>", "exec")))
exec(compile("9", "<c>", "eval"))


# exec nests, and nothing on the native stack does.
src = "print('bottom')"
for _ in range(20):
    src = "exec(" + repr(src) + ")"
exec(src)
