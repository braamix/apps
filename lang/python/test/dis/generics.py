type Pair[T] = tuple[T, T]


def first[T: int, *Ts, **P](x: T) -> T:
    return x


class Box[T = str](Base):
    def get(self) -> T:
        return self.item
