# A sequence on the left of + names the only thing it concatenates.
for f in [lambda: "b" + 1, lambda: [] + (), lambda: b"" + 1, lambda: () + []]:
    try: f()
    except TypeError as e: print(e)
x = "a"
try:
    x += 1
except TypeError as e: print(e)
class S(str): pass
try: S("a") + 1
except TypeError as e: print(e)
