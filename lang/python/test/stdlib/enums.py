# enum, over a class body that a Python mapping keeps.
import enum
from enum import Enum, IntEnum, Flag, IntFlag, auto, StrEnum, unique
class Color(Enum):
    RED = 1
    GREEN = 2
    BLUE = auto()
print(Color.RED, repr(Color.GREEN), Color(3), Color['RED'], list(Color), len(Color), Color.RED in Color)
print(Color.RED.name, Color.RED.value, Color.BLUE.value, type(Color.RED).__name__, isinstance(Color.RED, Color))
print(Color.RED is Color(1), Color.RED == 1, hash(Color.RED) == hash(Color.RED), {Color.RED: 'r'}[Color.RED])
class Num(IntEnum):
    ONE = 1
    TWO = 2
print(Num.ONE + 1, Num.TWO > Num.ONE, int(Num.TWO), Num(2), str(Num.ONE), format(Num.ONE))
class Perm(Flag):
    R = 4
    W = 2
    X = 1
rw = Perm.R | Perm.W
print(rw, Perm.R in rw, Perm.X in rw, list(rw), ~Perm.R, bool(Perm(0)), repr(rw))
class Mode(StrEnum):
    A = auto()
    B = 'bee'
print(Mode.A, Mode.B.upper(), Mode('bee'))
try:
    Color(9)
except ValueError as e:
    print(e)
try:
    @unique
    class Dup(Enum):
        A = 1
        B = 1
except ValueError as e:
    print(e)
print(Color.__members__.keys(), Dup if False else 0)
class Planet(Enum):
    EARTH = (5.97e24, 6.37e6)
    def __init__(self, mass, radius):
        self.mass = mass
    @property
    def heavy(self):
        return self.mass > 1e24
print(Planet.EARTH.heavy, Planet.EARTH.value)
import copy
print(copy.copy(Color.RED) is Color.RED, copy.deepcopy([Color.GREEN]))
