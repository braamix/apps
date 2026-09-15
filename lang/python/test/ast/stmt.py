import os
import os.path as p
from . import x
from ..pkg.mod import a as b, c
from x import *

def f(a, b=1, *args, c, d=2, **kw) -> int:
    global g
    x: int = 1
    y: int
    x += 1
    del x
    return a

@deco
@mod.deco(1)
class C(Base, metaclass=M, **kw):
    def m(self):
        pass

async def g2():
    await h()
    async with a as b:
        pass
    async for i in c:
        pass

for i in range(3):
    continue
else:
    pass

while True:
    break
else:
    pass

try:
    pass
except ValueError as e:
    raise
except (TypeError, KeyError):
    raise X from Y
else:
    pass
finally:
    pass

with open('a') as f, open('b'):
    pass

if a:
    pass
elif b:
    pass
else:
    pass

assert a, "msg"
a = b = c
a, b = b, a
(a, b), c = x
[a, b] = y
a; b
if a: b; c
