# ast over a native _ast: the tree, its positions, and what is built by hand.
import ast

SRC = '''\
import os.path as p
from . import q

@deco
class C(Base, metaclass=M):
    """doc"""
    x: int = 1
    __slots__ = ("a",)

    def m(self, a, /, b=2, *rest, k, **kw) -> "C":
        with open(a) as f, f as g:
            del self.x, b[0]
        for i, (j, *k) in enumerate(rest):
            while i:
                i -= 1
            else:
                continue
        try:
            assert a, "no"
        except (ValueError, TypeError) as e:
            raise RuntimeError from e
        finally:
            global gg
        try:
            pass
        except* OSError:
            pass
        return [y for y in b if y], {k: v for k, v in kw.items()}, {1, 2}


async def a(x):
    async with x as y:
        async for z in y:
            await z
    lam = lambda u, *, v=1: u if v else -u
    return (n := x.attr[1:2, ::3]) and not n or n @ n


type Alias[T: int, *Ts, **P = [int]] = dict[T, P]


def gen[T](v: T = 0) -> T:
    yield v
    r = yield from v
    match v:
        case [1, *rest] | (2, 3):
            pass
        case {"k": one, **more}:
            pass
        case C(1, y=2) as whole if whole:
            pass
        case _:
            pass
'''

tree = ast.parse(SRC)
print(ast.dump(tree, indent=1, include_attributes=True))
print("---")
print(ast.unparse(tree))
print("---")
print(ast.dump(ast.parse("f(*a, **b)", mode="eval")))
print(ast.dump(ast.parse("x", mode="single")))
print("---")
print(ast.literal_eval("{'a': [1, 2.5, (3j, b'x'), None, True, -4]}"))
print(ast.literal_eval(ast.parse("[1, 2]", mode="eval")))

# A tree built by hand, the way annotationlib builds one.
made = ast.Expression(
    body=ast.BinOp(
        left=ast.Name(id="a", ctx=ast.Load()),
        op=ast.Add(),
        right=ast.Call(
            func=ast.Attribute(value=ast.Name(id="m", ctx=ast.Load()), attr="f", ctx=ast.Load()),
            args=[ast.Constant(value=1)],
            keywords=[ast.keyword(arg="k", value=ast.Constant(value="s"))],
        ),
    )
)
print(ast.unparse(made))
print(ast.dump(made))

print("---")
for node in ast.walk(tree):
    if isinstance(node, ast.Name) and node.id == "gg":
        print("found", node.lineno, node.col_offset)

print(sorted(ast.Name._fields), ast.Name._attributes)
print(ast.stmt._attributes, ast.expr_context.__name__)
print(ast.Constant._field_types["value"], ast.Name._field_types["ctx"] is ast.expr_context)
print(issubclass(ast.Name, ast.expr), issubclass(ast.expr, ast.AST))
print(ast.iter_fields(ast.Constant(value=1)).__class__.__name__)
print([type(n).__name__ for n in ast.iter_child_nodes(ast.parse("a + b").body[0])])


class Renamer(ast.NodeTransformer):
    def visit_Name(self, node):
        return ast.copy_location(ast.Name(id=node.id.upper(), ctx=node.ctx), node)


t2 = ast.parse("a = b + c")
Renamer().visit(t2)
ast.fix_missing_locations(t2)
print(ast.unparse(t2))


class Counter(ast.NodeVisitor):
    def __init__(self):
        self.n = 0

    def visit_Constant(self, node):
        self.n += 1
        self.generic_visit(node)


c = Counter()
c.visit(ast.parse("[1, 2, 3, 'x']"))
print("constants", c.n)

# The message and the column a SyntaxError names are this parser's, not
# CPython's, so only the line is compared.
try:
    ast.parse("def f(:")
except SyntaxError as e:
    print("SyntaxError", e.lineno)
try:
    ast.literal_eval("f()")
except ValueError as e:
    print("ValueError", e)
