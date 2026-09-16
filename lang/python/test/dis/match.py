def f(x):
    match x:
        case 0 | 1:
            return "small"
        case [a, *rest] if a:
            return rest
        case {"k": v, **more}:
            return v, more
        case Point(x=0, y=py) as p:
            return p, py
        case str() | bytes():
            return "text"
        case None:
            return None
        case _:
            return x
