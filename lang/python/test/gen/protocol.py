# The generator object itself: what a call to a generator function makes, what
# resuming one does, and the four methods it answers.

def counter(n):
    print("entering")
    i = 0
    while i < n:
        i += 1
        yield i
    print("leaving")
    return n


print(type(counter(0)).__name__)
print(repr(counter(0))[:24])
print(list(counter(3)))

# A generator function runs none of its body until the first resume.
g = counter(2)
print("made")
print(next(g))
print(next(g))
try:
    next(g)
except StopIteration as e:
    print("stopped", e.args, e.value)
try:
    next(g)
except StopIteration as e:
    print("stopped again", e.args)

# next() with a default never raises.
print(next(counter(0), "empty"))

# iter() of a generator is the generator.
g = counter(1)
print(iter(g) is g, g.__iter__() is g)


# send: the value becomes what the yield expression is worth.
def echo():
    got = None
    while True:
        got = yield got
        if got == "stop":
            return "bye"


g = echo()
print(g.send(None))
print(g.send(1))
print(g.send("two"))
try:
    g.send("stop")
except StopIteration as e:
    print("echo", e.value)

# A just-started generator can only be sent None.
g = echo()
try:
    g.send(1)
except TypeError as e:
    print("TypeError", e)

# A running generator cannot be resumed.
def reenter():
    yield me.send(None)


me = reenter()
try:
    next(me)
except ValueError as e:
    print("ValueError", e)


# throw: the exception arrives at the yield.
def guarded():
    while True:
        try:
            yield "waiting"
        except KeyError as e:
            print("caught KeyError", e.args)


g = guarded()
print(next(g))
print(g.throw(KeyError))
print(g.throw(KeyError, "why"))
print(g.throw(KeyError("who")))

# Thrown into one that has not started, and into one that is finished.
g = guarded()
try:
    g.throw(ValueError("early"))
except ValueError as e:
    print("early", e.args)
g = counter(0)
list(g)
try:
    g.throw(ValueError("late"))
except ValueError as e:
    print("late", e.args)


# close: GeneratorExit at the yield, and no value may come back.
def cleanup():
    try:
        yield 1
        yield 2
    finally:
        print("cleaning up")


g = cleanup()
print(next(g))
print(g.close())
print(g.close())
print(cleanup().close())


def stubborn():
    try:
        yield 1
    except GeneratorExit:
        yield 2


g = stubborn()
next(g)
try:
    g.close()
except RuntimeError as e:
    print("RuntimeError", e)


# PEP 479: a StopIteration raised in the body is not the generator's end.
def bad():
    yield 1
    raise StopIteration("not an end")


g = bad()
print(next(g))
try:
    next(g)
except RuntimeError as e:
    print("PEP 479", e)


# An exception out of the body leaves the generator finished.
def explode():
    yield 1
    raise ValueError("boom")


g = explode()
print(next(g))
try:
    next(g)
except ValueError as e:
    print("boom", e.args)
try:
    next(g)
except StopIteration:
    print("finished")
