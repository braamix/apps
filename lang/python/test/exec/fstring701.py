# PEP 701: f-string fields hold any string, the same quote included.
x = 1
print(f"a{f"b{f"c{x}d"}e"}f")
print(f"{'a' if x else "b"}", f"{x:'>5}", f"{x!r:>{x+3}}")
print(f"{"\n".join(["p", "q"])}")
print(f"""{
    x  # a comment
    + 1
}""")
print(f'{x=}', f"{x = !r:^5}", f"{ {'k': 1}['k'] }", rf"\{x}\d")
print(f"{x:{"<"}4}|", f"{3 != 4}", f'{"""tri"ple"""}')
