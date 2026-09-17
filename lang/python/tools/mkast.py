#!/usr/bin/env python3
"""Write the expected parse tree of a source file, using CPython's ast module.

    tools/mkast.py [--regen] test/ast/expr.py [more...]

The golden is what `python --dump-ast` must print. The node kinds and their
fields are the ones parse.h defines, chosen to mirror CPython's own, so this is
a rename of `ast.parse`'s output and not a second parser.

It runs under $PYTHON when that is set; see tools/pyref.py.
"""

import ast
import os
import sys

import pyref

OPS = {
    ast.Add: "+", ast.Sub: "-", ast.Mult: "*", ast.Div: "/",
    ast.FloorDiv: "//", ast.Mod: "%", ast.Pow: "**",
    ast.BitAnd: "&", ast.BitOr: "|", ast.BitXor: "^",
    ast.LShift: "<<", ast.RShift: ">>", ast.MatMult: "@",
}
CMPS = {
    ast.Eq: "==", ast.NotEq: "!=", ast.Lt: "<", ast.LtE: "<=",
    ast.Gt: ">", ast.GtE: ">=", ast.In: "in", ast.NotIn: "not in",
    ast.Is: "is", ast.IsNot: "is not",
}
UNARY = {ast.Invert: "~", ast.Not: "not", ast.UAdd: "+", ast.USub: "-"}


class Out:
    def __init__(self):
        self.lines = []
        # The pieces of an f-string: the port re-scans them out of the
        # literal rather than in place, so their positions are its own.
        self.quiet = False

    def line(self, depth, text):
        self.lines.append("  " * depth + text)

    def field(self, depth, name, node):
        if node is None:
            self.line(depth, f"{name}: -")
        else:
            self.line(depth, f"{name}:")
            self.node(depth + 1, node)

    def listing(self, depth, name, items):
        if not items:
            self.line(depth, f"{name}: []")
            return
        self.line(depth, f"{name}:")
        for it in items:
            if it is None:
                self.line(depth + 1, "-")
            else:
                self.node(depth + 1, it)

    def node(self, d, n):
        k = type(n).__name__
        at = len(self.lines)
        getattr(self, "n_" + k, self.unknown)(d, n, k)
        # The nodes CPython gives a position to carry it on their header.
        if hasattr(n, "lineno") and not self.quiet:
            self.lines[at] += " @%d:%d-%d:%d" % (
                n.lineno, n.col_offset, n.end_lineno, n.end_col_offset)

    def unknown(self, d, n, k):
        raise SystemExit(f"mkast: {k} is not handled")

    # ------------------------------------------------------------ statements

    def n_Module(self, d, n, _k):
        self.line(d, "Module")
        self.listing(d + 1, "body", n.body)

    def type_params(self, d, n):
        # Printed only where there are some, so a golden from before 3.12 stands.
        if getattr(n, "type_params", None):
            self.listing(d, "type_params", n.type_params)

    def _funcdef(self, d, n, kind):
        self.line(d, f"{kind} {n.name}")
        self.field(d + 1, "args", n.args)
        self.listing(d + 1, "body", n.body)
        self.listing(d + 1, "decorators", n.decorator_list)
        self.field(d + 1, "returns", n.returns)
        self.type_params(d + 1, n)

    def n_FunctionDef(self, d, n, _k):
        self._funcdef(d, n, "FunctionDef")

    def n_AsyncFunctionDef(self, d, n, _k):
        self._funcdef(d, n, "AsyncFunctionDef")

    def n_ClassDef(self, d, n, _k):
        self.line(d, f"ClassDef {n.name}")
        self.listing(d + 1, "bases", n.bases)
        self.listing(d + 1, "keywords", n.keywords)
        self.listing(d + 1, "body", n.body)
        self.listing(d + 1, "decorators", n.decorator_list)
        self.type_params(d + 1, n)

    def n_Return(self, d, n, _k):
        self.line(d, "Return")
        self.field(d + 1, "value", n.value)

    def n_Delete(self, d, n, _k):
        self.line(d, "Delete")
        self.listing(d + 1, "targets", n.targets)

    def n_Assign(self, d, n, _k):
        self.line(d, "Assign")
        self.listing(d + 1, "targets", n.targets)
        self.field(d + 1, "value", n.value)

    def n_AugAssign(self, d, n, _k):
        self.line(d, f"AugAssign {OPS[type(n.op)]}=")
        self.field(d + 1, "target", n.target)
        self.field(d + 1, "value", n.value)

    def n_AnnAssign(self, d, n, _k):
        self.line(d, "AnnAssign simple" if n.simple else "AnnAssign")
        self.field(d + 1, "target", n.target)
        self.field(d + 1, "annotation", n.annotation)
        self.field(d + 1, "value", n.value)

    def _for(self, d, n, kind):
        self.line(d, kind)
        self.field(d + 1, "target", n.target)
        self.field(d + 1, "iter", n.iter)
        self.listing(d + 1, "body", n.body)
        self.listing(d + 1, "orelse", n.orelse)

    def n_For(self, d, n, _k):
        self._for(d, n, "For")

    def n_AsyncFor(self, d, n, _k):
        self._for(d, n, "AsyncFor")

    def _test_body_else(self, d, n, kind):
        self.line(d, kind)
        self.field(d + 1, "test", n.test)
        self.listing(d + 1, "body", n.body)
        self.listing(d + 1, "orelse", n.orelse)

    def n_While(self, d, n, _k):
        self._test_body_else(d, n, "While")

    def n_If(self, d, n, _k):
        self._test_body_else(d, n, "If")

    def _with(self, d, n, kind):
        self.line(d, kind)
        self.listing(d + 1, "items", n.items)
        self.listing(d + 1, "body", n.body)

    def n_With(self, d, n, _k):
        self._with(d, n, "With")

    def n_AsyncWith(self, d, n, _k):
        self._with(d, n, "AsyncWith")

    def n_Raise(self, d, n, _k):
        self.line(d, "Raise")
        self.field(d + 1, "exc", n.exc)
        self.field(d + 1, "cause", n.cause)

    def n_Try(self, d, n, k):
        self.line(d, k)
        self.listing(d + 1, "body", n.body)
        self.listing(d + 1, "handlers", n.handlers)
        self.listing(d + 1, "orelse", n.orelse)
        self.listing(d + 1, "finalbody", n.finalbody)

    n_TryStar = n_Try

    def n_TypeAlias(self, d, n, _k):
        self.line(d, "TypeAlias")
        self.field(d + 1, "name", n.name)
        self.listing(d + 1, "type_params", n.type_params)
        self.field(d + 1, "value", n.value)

    def n_TypeVar(self, d, n, _k):
        self.line(d, f"TypeVar {n.name}")
        self.field(d + 1, "bound", n.bound)
        self.field(d + 1, "default", n.default_value)

    def n_ParamSpec(self, d, n, k):
        self.line(d, f"{k} {n.name}")
        self.field(d + 1, "default", n.default_value)

    n_TypeVarTuple = n_ParamSpec

    # ----------------------------------------------------------------- match

    def n_Match(self, d, n, _k):
        self.line(d, "Match")
        self.field(d + 1, "subject", n.subject)
        self.listing(d + 1, "cases", n.cases)

    def n_match_case(self, d, n, _k):
        self.line(d, "MatchCase")
        self.field(d + 1, "pattern", n.pattern)
        self.field(d + 1, "guard", n.guard)
        self.listing(d + 1, "body", n.body)

    def n_MatchValue(self, d, n, _k):
        self.line(d, "MatchValue")
        self.field(d + 1, "value", n.value)

    def n_MatchSingleton(self, d, n, _k):
        self.line(d, f"MatchSingleton {n.value!r}")

    def n_MatchSequence(self, d, n, k):
        self.line(d, k)
        self.listing(d + 1, "patterns", n.patterns)

    n_MatchOr = n_MatchSequence

    def n_MatchMapping(self, d, n, _k):
        self.line(d, f"MatchMapping **{n.rest}" if n.rest else "MatchMapping")
        self.listing(d + 1, "keys", n.keys)
        self.listing(d + 1, "patterns", n.patterns)

    def n_MatchClass(self, d, n, _k):
        self.line(d, "MatchClass")
        self.field(d + 1, "cls", n.cls)
        self.listing(d + 1, "patterns", n.patterns)
        self.line(d + 1, "keywords:" if n.kwd_attrs else "keywords: []")
        for name, p in zip(n.kwd_attrs, n.kwd_patterns):
            self.line(d + 2, f"Keyword {name}")
            self.field(d + 3, "value", p)

    def n_MatchStar(self, d, n, _k):
        self.line(d, f"MatchStar {n.name}" if n.name else "MatchStar")

    def n_MatchAs(self, d, n, _k):
        self.line(d, f"MatchAs {n.name}" if n.name else "MatchAs")
        self.field(d + 1, "pattern", n.pattern)

    def n_Assert(self, d, n, _k):
        self.line(d, "Assert")
        self.field(d + 1, "test", n.test)
        self.field(d + 1, "msg", n.msg)

    def n_Import(self, d, n, _k):
        self.line(d, "Import lazy" if getattr(n, "is_lazy", 0) else "Import")
        self.listing(d + 1, "names", n.names)

    def n_ImportFrom(self, d, n, _k):
        lazy = " lazy" if getattr(n, "is_lazy", 0) else ""
        self.line(d, f"ImportFrom {n.module or '-'} level={n.level}{lazy}")
        self.listing(d + 1, "names", n.names)

    def n_Global(self, d, n, _k):
        self.line(d, "Global")
        self.line(d + 1, "names:")
        for name in n.names:
            self.line(d + 2, f"Name {name}")

    def n_Nonlocal(self, d, n, _k):
        self.line(d, "Nonlocal")
        self.line(d + 1, "names:")
        for name in n.names:
            self.line(d + 2, f"Name {name}")

    def n_Expr(self, d, n, _k):
        self.line(d, "Expr")
        self.field(d + 1, "value", n.value)

    def n_Pass(self, d, _n, _k):
        self.line(d, "Pass")

    def n_Break(self, d, _n, _k):
        self.line(d, "Break")

    def n_Continue(self, d, _n, _k):
        self.line(d, "Continue")

    # ----------------------------------------------------------- expressions

    def n_BoolOp(self, d, n, _k):
        self.line(d, "BoolOp " + ("and" if isinstance(n.op, ast.And) else "or"))
        self.listing(d + 1, "values", n.values)

    def n_NamedExpr(self, d, n, _k):
        self.line(d, "NamedExpr")
        self.field(d + 1, "target", n.target)
        self.field(d + 1, "value", n.value)

    def n_BinOp(self, d, n, _k):
        self.line(d, f"BinOp {OPS[type(n.op)]}")
        self.field(d + 1, "left", n.left)
        self.field(d + 1, "right", n.right)

    def n_UnaryOp(self, d, n, _k):
        self.line(d, f"UnaryOp {UNARY[type(n.op)]}")
        self.field(d + 1, "operand", n.operand)

    def n_Lambda(self, d, n, _k):
        self.line(d, "Lambda")
        self.field(d + 1, "args", n.args)
        self.field(d + 1, "body", n.body)

    def n_IfExp(self, d, n, _k):
        self.line(d, "IfExp")
        self.field(d + 1, "test", n.test)
        self.field(d + 1, "body", n.body)
        self.field(d + 1, "orelse", n.orelse)

    def n_Dict(self, d, n, _k):
        self.line(d, "Dict")
        self.listing(d + 1, "keys", n.keys)
        self.listing(d + 1, "values", n.values)

    def n_Set(self, d, n, _k):
        self.line(d, "Set")
        self.listing(d + 1, "elts", n.elts)

    def _comp(self, d, n, kind):
        self.line(d, kind)
        self.field(d + 1, "elt", n.elt)
        self.listing(d + 1, "generators", n.generators)

    def n_ListComp(self, d, n, _k):
        self._comp(d, n, "ListComp")

    def n_SetComp(self, d, n, _k):
        self._comp(d, n, "SetComp")

    def n_GeneratorExp(self, d, n, _k):
        self._comp(d, n, "GeneratorExp")

    def n_DictComp(self, d, n, _k):
        self.line(d, "DictComp")
        self.field(d + 1, "key", n.key)
        self.field(d + 1, "value", n.value)
        self.listing(d + 1, "generators", n.generators)

    def n_Await(self, d, n, _k):
        self.line(d, "Await")
        self.field(d + 1, "value", n.value)

    def n_Yield(self, d, n, _k):
        self.line(d, "Yield")
        self.field(d + 1, "value", n.value)

    def n_YieldFrom(self, d, n, _k):
        self.line(d, "YieldFrom")
        self.field(d + 1, "value", n.value)

    def n_Compare(self, d, n, _k):
        self.line(d, "Compare")
        self.field(d + 1, "left", n.left)
        self.line(d + 1, "ops:")
        for op, rhs in zip(n.ops, n.comparators):
            self.line(d + 2, f"CmpOp {CMPS[type(op)]}")
            self.field(d + 3, "operand", rhs)

    def n_Call(self, d, n, _k):
        self.line(d, "Call")
        self.field(d + 1, "func", n.func)
        self.listing(d + 1, "args", n.args)
        self.listing(d + 1, "keywords", n.keywords)

    def n_Constant(self, d, n, _k):
        self.line(d, f"Constant {constant_text(n.value)}")

    def n_Attribute(self, d, n, _k):
        self.line(d, f"Attribute {n.attr}")
        self.field(d + 1, "value", n.value)

    def n_Subscript(self, d, n, _k):
        self.line(d, "Subscript")
        self.field(d + 1, "value", n.value)
        self.field(d + 1, "slice", n.slice)

    def n_Index(self, d, n, _k):
        # 3.8 wraps a plain subscript in Index; 3.9 does not.
        self.node(d, n.value)

    def n_ExtSlice(self, d, n, _k):
        self.line(d, "Tuple")
        self.listing(d + 1, "elts", n.dims)

    def n_Starred(self, d, n, _k):
        self.line(d, "Starred")
        self.field(d + 1, "value", n.value)

    def n_Name(self, d, n, _k):
        self.line(d, f"Name {n.id}")

    def n_List(self, d, n, _k):
        self.line(d, "List")
        self.listing(d + 1, "elts", n.elts)

    def n_Tuple(self, d, n, _k):
        self.line(d, "Tuple")
        self.listing(d + 1, "elts", n.elts)

    def n_Slice(self, d, n, _k):
        self.line(d, "Slice")
        self.field(d + 1, "lower", n.lower)
        self.field(d + 1, "upper", n.upper)
        self.field(d + 1, "step", n.step)

    def quiet_listing(self, d, name, items):
        was, self.quiet = self.quiet, True
        self.listing(d, name, items)
        self.quiet = was

    def n_JoinedStr(self, d, n, _k):
        self.line(d, "JoinedStr")
        self.quiet_listing(d + 1, "values", n.values)

    def n_FormattedValue(self, d, n, _k):
        # -1 is "no conversion"; otherwise it is the character itself.
        conv = chr(n.conversion) if n.conversion and n.conversion > 0 else "-"
        self.line(d, f"FormattedValue {conv}")
        self.field(d + 1, "value", n.value)
        self.field(d + 1, "format_spec", n.format_spec)

    def n_TemplateStr(self, d, n, _k):
        self.line(d, "TemplateStr")
        self.quiet_listing(d + 1, "values", n.values)

    def n_Interpolation(self, d, n, _k):
        conv = chr(n.conversion) if n.conversion and n.conversion > 0 else "-"
        self.line(d, f"Interpolation {conv}")
        self.field(d + 1, "value", n.value)
        self.field(d + 1, "str", ast.Constant(n.str))
        self.field(d + 1, "format_spec", n.format_spec)

    # --------------------------------------------------------------- helpers

    def n_comprehension(self, d, n, _k):
        self.line(d, "Comprehen async" if n.is_async else "Comprehen")
        self.field(d + 1, "target", n.target)
        self.field(d + 1, "iter", n.iter)
        self.listing(d + 1, "ifs", n.ifs)

    def n_ExceptHandler(self, d, n, _k):
        self.line(d, f"ExceptHandler {n.name}" if n.name else "ExceptHandler")
        self.field(d + 1, "type", n.type)
        self.listing(d + 1, "body", n.body)

    def n_arguments(self, d, n, _k):
        self.line(d, "Arguments")
        self.listing(d + 1, "posonly", getattr(n, "posonlyargs", []))
        self.listing(d + 1, "args", n.args)
        self.field(d + 1, "vararg", n.vararg)
        self.listing(d + 1, "kwonly", n.kwonlyargs)
        self.listing(d + 1, "kw_defaults", n.kw_defaults)
        self.field(d + 1, "kwarg", n.kwarg)
        self.listing(d + 1, "defaults", n.defaults)

    def n_arg(self, d, n, _k):
        self.line(d, f"Arg {n.arg}")
        self.field(d + 1, "annotation", n.annotation)

    def n_keyword(self, d, n, _k):
        self.line(d, f"Keyword {n.arg}" if n.arg else "Keyword **")
        self.field(d + 1, "value", n.value)

    def n_alias(self, d, n, _k):
        self.line(d, f"Alias {n.name} as {n.asname}" if n.asname else f"Alias {n.name}")

    def n_withitem(self, d, n, _k):
        self.line(d, "WithItem")
        self.field(d + 1, "context", n.context_expr)
        self.field(d + 1, "vars", n.optional_vars)


def constant_text(v):
    if v is Ellipsis:
        return "Ellipsis"
    return repr(v)


def main():
    pyref.reexec()
    paths, regen = pyref.args()
    if not paths:
        raise SystemExit(__doc__)
    for path in paths:
        pyref.check(path, pyref.tag(), regen)
        with open(path, "rb") as f:
            source = f.read()
        out = Out()
        out.node(0, ast.parse(source))
        text = "".join(s + "\n" for s in out.lines)
        with open(path + ".exp", "w", encoding="utf-8") as f:
            f.write(text)
        pyref.record(path, pyref.tag())
        print(f"mkast: {os.path.basename(path)}: {len(out.lines)} lines")


if __name__ == "__main__":
    main()
