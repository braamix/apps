# string, string.templatelib and types. string.Template waits for re.
import string
import types
from string.templatelib import Template, Interpolation

print(string.ascii_letters[:5], string.digits, string.punctuation[:5], string.capwords("hello  big world"))
f = string.Formatter()
print(f.format("{0}-{name!r:>6}-{1[0]}", "a", ["b"], name="n"), list(f.parse("x{0:>3}y")))


class Upper(string.Formatter):
    def format_field(self, value, spec):
        return str(value).upper()


print(Upper().format("{} and {x}", "a", x="b"))
tpl = t"{1 + 1} is {'two'!r}"
print(isinstance(tpl, Template), [type(p).__name__ for p in tpl], tpl.strings, tpl.values)
print(types.SimpleNamespace(a=1), types.MappingProxyType({"k": 1})["k"], types.FunctionType,
      types.BuiltinFunctionType, types.MethodType)
C = types.new_class("C", (), {}, lambda ns: ns.update(x=5))
print(C.__name__, C.x, types.resolve_bases((int,)), types.get_original_bases(C))


@types.coroutine
def gen_coro():
    yield 1


async def outer():
    await gen_coro()


co = outer()
print(co.send(None), isinstance(co, types.CoroutineType))
co.close()
m = types.ModuleType("mm", "the doc")
print(m, m.__doc__, types.GenericAlias(list, (int,)), types.NoneType, types.EllipsisType)
print(types.prepare_class("D", (), {"metaclass": type})[0])
