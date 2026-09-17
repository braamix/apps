# atexit: the calls run last registered first, after the program's own
# output, and unregister takes every call of a function.
import atexit


def bye(who, punct="!"):
    print("bye", who + punct)


atexit.register(bye, "one")
atexit.register(bye, "two", punct="?")
atexit.register(bye, "never")
atexit.unregister(bye)
atexit.register(bye, "three")
print("main", atexit._ncallbacks())
