# typing over a native _typing, and dataclasses over annotationlib.
import typing
from typing import (List, Dict, Optional, Union, Any, Callable, Tuple, TypeVar,
                    Generic, Protocol, runtime_checkable, NamedTuple, TypedDict,
                    get_type_hints, get_origin, get_args, cast, Final, ClassVar,
                    Literal, Annotated, overload, TYPE_CHECKING, Self, Never)
T = TypeVar("T")
print(List[int], Dict[str, int], Optional[int], Union[int, str])
print(get_origin(List[int]), get_args(Dict[str, int]))
class Box(Generic[T]):
    def __init__(self, v: T) -> None: self.v = v
    def get(self) -> T: return self.v
print(Box[int], Box[int]().__class__ if False else Box(3).get())
print(Box.__parameters__)

@runtime_checkable
class Sized(Protocol):
    def __len__(self) -> int: ...
print(isinstance([1,2], Sized), isinstance(3, Sized))

class Point(NamedTuple):
    x: int
    y: int = 0
p = Point(1)
print(p, p.x, p.y, Point._fields)

class Movie(TypedDict):
    name: str
    year: int
print(Movie.__annotations__, Movie.__total__)

def f(a: "int", b: List[str]) -> "Box[int]": ...
print(get_type_hints(f))
print(Annotated[int, "meta"], Literal[1, 2], Final, ClassVar[int])
print(typing.get_type_hints(Box.get))
print(Callable[[int, str], bool])
print(Tuple[int, ...])
X = typing.NewType("X", int)
print(X, X(3))
print(typing.cast(int, "3"))
type Alias[T] = list[T]
# The repr of a subscripted alias gained its module in 3.15, and the language
# this tracks is 3.14's, so what it is made of is printed instead.
print(Alias, typing.get_origin(Alias[int]), typing.get_args(Alias[int]), Alias.__value__)

# dataclasses, which is the first thing most code wants annotations for.
from dataclasses import dataclass, field, fields, asdict, astuple, replace


@dataclass(order=True)
class P:
    x: int
    y: int = 0
    tags: list = field(default_factory=list)


p = P(1, 2)
print(p, p == P(1, 2), p < P(2, 0))
print([f.name for f in fields(p)], asdict(p), astuple(p))
print(replace(p, y=9))


@dataclass(frozen=True, slots=True)
class Q:
    a: str = "s"


print(Q(), hash(Q()) == hash(Q()))
print(P.__annotations__, [f.type for f in fields(P)])
