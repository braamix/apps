# PEP 750: t-strings make a Template, not a str.
x, y, w = 1, "two", 4
t = t"a{x}b{y!r:>{w}}c"
print(t)
print(t.strings, t.interpolations, t.values)
print(list(t), list(t"{x}{y}"), list(t""))
print(t"{x=}", t"{ x = }", t"{x =:3}")
print(t"lit" t" {x}" t"""{y}""", rt"\n{x}")
Template = type(t)
Interpolation = type(t.interpolations[0])
print(Template.__name__, Template.__module__, Interpolation.__qualname__)
print(Interpolation.__match_args__)
print(Template("a", Interpolation(1, "x"), "b", "c", Interpolation(2)))
print(Template(Interpolation(1, "x", "r", "0>3")))
i = Interpolation(value=5, format_spec=">2")
print(i.value, repr(i.expression), i.conversion, repr(i.format_spec))
print(t"a" + t"b{x}", (t"{x}" + t"{y}").strings)
for bad in [lambda: t"a" + "b", lambda: "b" + t"a", lambda: Template(1),
            lambda: Interpolation(1, 2), lambda: Interpolation(1, "x", "q"),
            lambda: Interpolation(), lambda: Template(a="b")]:
    try:
        bad()
    except (TypeError, ValueError) as e:
        print(type(e).__name__, e)
match t"{x}":
    case Template(strings=("", ""), interpolations=(Interpolation(v, e, c, s),)):
        print("matched", v, e, c, repr(s))
print(Template[int], Interpolation[str])
print(isinstance(t, Template), type(iter(t)).__name__)
def render(tmpl):
    out = []
    for part in tmpl:
        if isinstance(part, str):
            out.append(part)
        else:
            v = part.value
            if part.conversion == "r":
                v = repr(v)
            out.append(format(v, part.format_spec))
    return "".join(out)
print(render(t"<{x:03}|{y!r}|{[x, y]}>"))
print(t"{t"{x}"}", t"{f"{x}"}")
print(t"{x:{w}.{w}}".interpolations[0].format_spec)
