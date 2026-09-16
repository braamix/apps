def f[T](x): pass
async def g[T: int, *Ts, **P](*a: *Ts, **k): pass
class C[T = int, *Ts = *tuple[int], **P = [int, str]](Base, metaclass=M):
    def m[U: (int, str)](self): pass
@deco
class D[T]: pass
type A = int
type B[K, V: str] = dict[K, V]
type = 1
type[x] = 2
type.x = 3
print(type)
