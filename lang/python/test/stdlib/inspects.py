# inspect over dis, annotationlib and ast.
import inspect


def plain(a, b=1, *rest, kw, other=2, **more) -> bool:
    """A docstring.

    Indented.
    """
    return True


async def coro(x: int):
    pass


def gen():
    yield 1


class C:
    attr = 1

    def method(self, a: "C") -> None:
        pass

    @classmethod
    def cm(cls):
        pass

    @staticmethod
    def sm():
        pass

    @property
    def prop(self):
        return 2


sig = inspect.signature(plain)
print(sig)
print(list(sig.parameters))
for name, p in sig.parameters.items():
    print(name, p.kind, p.default is inspect.Parameter.empty, p.annotation)
print(sig.return_annotation)
print(sig.bind(1, 2, 3, kw=4).arguments)

print(inspect.getdoc(plain))
print(inspect.cleandoc("  a\n    b\n  c"))

print(inspect.isfunction(plain), inspect.isgeneratorfunction(gen))
print(inspect.iscoroutinefunction(coro), inspect.isclass(C))
print(inspect.ismethod(C().method), inspect.isbuiltin(len))
print(inspect.isabstract(C), inspect.ismodule(inspect))

print([n for n, _ in inspect.getmembers(C, inspect.isfunction)])
print(inspect.getmro(bool))
print(inspect.signature(C.method))
print(inspect.get_annotations(C.method))
print(inspect.get_annotations(coro))

print(inspect.getfullargspec(plain))
print(inspect.formatargvalues(*inspect.getargvalues(inspect.currentframe()))[:1])


def outer():
    v = 1

    def inner():
        return v

    return inner


print(inspect.getclosurevars(outer()).nonlocals)
print(inspect.unwrap(plain) is plain)
print(inspect.signature(lambda *, a, b=2: None))

# A signature built and changed by hand, which is what a decorator does.
s2 = sig.replace(return_annotation=int)
print(s2.return_annotation)
p0 = inspect.Parameter("z", inspect.Parameter.POSITIONAL_OR_KEYWORD, default=5)
print(inspect.Signature([p0]))
print(inspect.BoundArguments is not None)
