match x:
    case 1 | 2 as y if y:
        pass
    case [a, *rest]:
        pass
    case {"k": v, **kw}:
        pass
    case Point(1, y=2):
        pass
    case None:
        pass
    case -1 + 2j:
        pass
    case a.b:
        pass
    case (1, 2) | ():
        pass
    case b"x" b"y":
        pass
    case {1: _, -2: [*_], a.b: (c)}:
        pass
    case x, *y, z:
        pass
    case _:
        pass
match (a, b):
    case q:
        pass
match a, *b:
    case q if q:
        pass
match = 1
match[x]: int = 2
match(x).y
case = [match, case]
