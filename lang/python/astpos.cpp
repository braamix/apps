// The token span of every node; see astpos.h.
#include "astpos.h"

#include "err.h"

namespace {

// Which of a, b, c and d hold a node index rather than a count or a token.
enum : u8 { KA = 1, KB = 2, KC = 4, KD = 8 };

u8 kid_fields(Nd k)
{
    switch (k) {
    case Nd::FunctionDef:
    case Nd::AsyncFunctionDef:
        return KA | KD;
    case Nd::Return:
    case Nd::Await:
    case Nd::Yield:
    case Nd::YieldFrom:
    case Nd::Expr:
    case Nd::Starred:
    case Nd::Assign:
    case Nd::UnaryOp:
    case Nd::ListComp:
    case Nd::SetComp:
    case Nd::GeneratorExp:
    case Nd::Compare:
    case Nd::CmpOp:
    case Nd::Call:
    case Nd::Attribute:
    case Nd::ExceptHandler:
    case Nd::Arg:
    case Nd::Keyword:
    case Nd::Match:
    case Nd::MatchValue:
    case Nd::MatchAs:
    case Nd::MatchClass:
    case Nd::While:
    case Nd::If:
        return KA;
    case Nd::AugAssign:
    case Nd::NamedExpr:
    case Nd::BinOp:
    case Nd::For:
    case Nd::AsyncFor:
    case Nd::Raise:
    case Nd::Assert:
    case Nd::Lambda:
    case Nd::DictComp:
    case Nd::FormattedValue:
    case Nd::Subscript:
    case Nd::Comprehen:
    case Nd::WithItem:
    case Nd::MatchCase:
    case Nd::TypeAlias:
    case Nd::TypeVar:
        return KA | KB;
    case Nd::AnnAssign:
    case Nd::IfExp:
    case Nd::Interpolation:
    case Nd::Slice:
        return KA | KB | KC;
    case Nd::ParamSpec:
    case Nd::TypeVarTuple:
        return KB;
    default:
        return 0;
    }
}

constexpr u32 NONE = u32(-1);

bool is_opener(Tok k)
{
    return k == Tok::LPar || k == Tok::LSqb || k == Tok::LBrace;
}

bool is_closer(Tok k)
{
    return k == Tok::RPar || k == Tok::RSqb || k == Tok::RBrace;
}

struct Spanner {
    Ast *ast;
    Vec<u32> opener; // a closing bracket -> the token that opened it
    Vec<u32> closer; // and back again

    bool brackets()
    {
        usize n = ast->lex.tokens.size();
        if (!opener.resize(n) || !closer.resize(n))
            return false;
        Vec<u32> stack;
        for (usize k = 0; k < n; k++) {
            Tok t = ast->lex.tokens[k].kind;
            if (is_opener(t)) {
                if (!stack.push(u32(k)))
                    return false;
            } else if (is_closer(t) && stack.size()) {
                opener[k]            = stack.back();
                closer[stack.back()] = u32(k);
                stack.pop();
            }
        }
        return true;
    }

    Tok kind_at(u32 k) const
    {
        return k < ast->lex.tokens.size() ? ast->lex.tokens[k].kind : Tok::End;
    }

    // The nearest token of `want` at or before `from`. NONE for none.
    u32 back_to(u32 from, Tok want) const
    {
        for (u32 k = from + 1; k > 0; k--)
            if (kind_at(k - 1) == want)
                return k - 1;
        return NONE;
    }

    void widen(u32 &first, u32 &last, u32 child)
    {
        if (!child)
            return;
        NodeSpan s = walk(child);
        if (s.first < first)
            first = s.first;
        if (s.last > last)
            last = s.last;
    }

    u32 keyword_start(const Node &n, u32 tok) const;
    void widen_ends(const Node &n, u32 &first, u32 &last) const;
    NodeSpan walk(u32 i);
};

// The token a node really begins at, where the parser points elsewhere. The
// answer is NONE where what is under the node already says.
u32 Spanner::keyword_start(const Node &n, u32 tok) const
{
    switch (n.kind) {
    // A def is at `def`, an `async def` two tokens earlier still, a class at
    // `class`; and their decorators are not part of them, so these three are
    // set rather than taken from what is under them.
    case Nd::FunctionDef:
        return tok >= 1 ? tok - 1 : NONE;
    case Nd::AsyncFunctionDef:
        return tok >= 2 ? tok - 2 : NONE;
    case Nd::ClassDef:
        return tok >= 1 ? tok - 1 : NONE;
    // A starred form is at the star the parser stepped over. A `*_` has no
    // name, so its own token is already the star.
    case Nd::MatchStar:
        return (n.flags & 1) && tok >= 1 ? tok - 1 : NONE;
    case Nd::TypeVarTuple:
    case Nd::ParamSpec:
        return tok >= 1 ? tok - 1 : NONE;
    // A keyword the node's own token is past.
    case Nd::ExceptHandler:
        return back_to(tok, Tok::KwExcept);
    case Nd::ImportFrom: {
        u32 from = back_to(tok, Tok::KwFrom);
        return (n.pad & 1) && from != NONE && from >= 1 ? from - 1 : from;
    }
    // `async for` and `async with` are at the `async`.
    case Nd::AsyncFor:
    case Nd::AsyncWith:
        return tok >= 1 ? tok - 1 : NONE;
    // `lazy import x` is at the `lazy` (PEP 810).
    case Nd::Import:
        return (n.pad & 1) && tok >= 1 ? tok - 1 : NONE;
    default:
        return NONE;
    }
}

// What a node reaches past its children on the right.
void Spanner::widen_ends(const Node &n, u32 &first, u32 &last) const
{
    switch (n.kind) {
    // An alias covers the whole dotted name and the `as` behind it.
    case Nd::Alias:
        if (n.b > last + 1)
            last = n.b - 1;
        if (n.a && n.a - 1 > last)
            last = n.a - 1;
        break;
    // `a[1:]` ends after the colon, which is not a child of anything, and
    // `a[(x+1):]` reaches over the parentheses its lower was written in.
    case Nd::Slice:
        for (;;) {
            if (kind_at(last + 1) == Tok::Colon) {
                last++;
                continue;
            }
            if (first && kind_at(first - 1) == Tok::LPar && closer[first - 1] == last + 1) {
                first--;
                last++;
                continue;
            }
            break;
        }
        break;
    // Adjacent literals are one constant, and it covers all of them.
    case Nd::Constant:
        while (kind_at(last + 1) == Tok::Str || kind_at(last + 1) == Tok::Bytes ||
               kind_at(last + 1) == Tok::FStr)
            last++;
        break;
    // `*_` binds nothing, so the wildcard behind the star is not recorded.
    case Nd::MatchStar:
        if (!(n.flags & 1) && kind_at(last + 1) == Tok::Name)
            last++;
        break;
    // `del (a), [b],` ends at the comma nothing else claims.
    case Nd::Delete:
        while (kind_at(last + 1) == Tok::Comma)
            last++;
        break;
    // A mapping pattern is written in braces the parser did not point at.
    case Nd::MatchMapping: {
        u32 brace = back_to(first, Tok::LBrace);
        if (brace != NONE && closer[brace] >= last) {
            first = brace;
            last  = closer[brace];
        }
        break;
    }
    default:
        break;
    }
}

NodeSpan Spanner::walk(u32 i)
{
    if (!i || i >= ast->nodes.size())
        return NodeSpan{ 0, 0 };
    NodeSpan &out = ast->spans[i];
    if (out.first || out.last)
        return out;

    const Node &n = ast->at(i);
    // A tuple is written with no brackets of its own unless it is in
    // parentheses, so the `[` a subscript put it at is not part of it.
    bool bare = n.kind == Nd::Tuple && n.nkid;
    u32 first = bare ? u32(-1) : n.tok, last = bare ? 0 : n.tok;

    // The pieces of an f-string are lexed out of the literal rather than in
    // place, so their tokens say nothing about where the string is: the
    // literal itself, and any literal written beside it, is the whole span.
    if (n.kind == Nd::JoinedStr || n.kind == Nd::TemplateStr) {
        while (kind_at(last + 1) == Tok::Str || kind_at(last + 1) == Tok::Bytes ||
               kind_at(last + 1) == Tok::FStr)
            last++;
        out = NodeSpan{ first, last };
        return out;
    }

    u8 mask = kid_fields(n.kind);
    if (mask & KA)
        widen(first, last, n.a);
    if (mask & KB)
        widen(first, last, n.b);
    if (mask & KC)
        widen(first, last, n.c);
    if (mask & KD)
        widen(first, last, n.d);
    for (u32 k = 0; k < n.nkid; k++)
        widen(first, last, ast->kids[n.kid0 + k]);

    u32 fixed = keyword_start(n, n.tok);
    if (fixed != NONE)
        first = fixed;
    widen_ends(n, first, last);

    // Brackets. One that opened inside this node and closes just after it is
    // part of it, so `f(a)` ends at the `)`; one that opened just before it
    // and closes inside it is too, so `(1 + 2) * 3` begins at the `(`. A `(`
    // around a name closes past the whole node and is not part of it -- but a
    // generator expression is the one thing written in the parentheses that
    // hold it, so for that kind it is.
    bool again = true;
    while (again) {
        again = false;
        for (;;) {
            // A trailing comma before the bracket is part of it too.
            u32 j = last + 1;
            while (kind_at(j) == Tok::Comma)
                j++;
            if (!is_closer(kind_at(j)) || opener[j] < first || opener[j] > last)
                break;
            last  = j;
            again = true;
        }
        if (first && is_opener(kind_at(first - 1)) && closer[first - 1] > first &&
            closer[first - 1] <= last) {
            first--;
            again = true;
        }
    }
    // A generator expression is the parentheses that hold it, and so is a
    // tuple that was written in some.
    bool parens = false;
    // A generator expression the parser put at its own `(` has its
    // parentheses already; the next pair out belongs to the call round it.
    bool wrapped = !bare && kind_at(first) == Tok::LPar && closer[first] == last;
    if ((n.kind == Nd::GeneratorExp || bare) && !wrapped && first &&
        kind_at(first - 1) == Tok::LPar) {
        u32 j = last + 1;
        while (kind_at(j) == Tok::Comma)
            j++;
        if (closer[first - 1] == j) {
            first  = first - 1;
            last   = j;
            parens = true;
        }
    }
    // A tuple written without parentheses ends at the comma that made it one.
    if (bare && !parens)
        while (kind_at(last + 1) == Tok::Comma)
            last++;
    // The reach past the children is taken again: a bracket absorbed above
    // may have put a colon or a comma within sight that was not before.
    widen_ends(n, first, last);
    // `case +1 + 2j`: the sign belongs to the sum, not to the number under it.
    if (n.kind == Nd::MatchValue && n.a && ast->at(n.a).kind == Nd::BinOp)
        ast->spans[n.a].first = first;

    out = NodeSpan{ first, last };
    return out;
}

} // namespace

bool ast_spans(Ast &ast)
{
    if (ast.spans.size() == ast.nodes.size())
        return true;
    if (!ast.spans.resize(ast.nodes.size()))
        return err_set("MemoryError", "out of memory"), false;
    Spanner s{ &ast, Vec<u32>(), Vec<u32>() };
    if (!s.brackets())
        return err_set("MemoryError", "out of memory"), false;
    for (u32 i = 1; i < ast.nodes.size(); i++)
        s.walk(i);
    return true;
}

Pos ast_pos(const Ast &ast, u32 node)
{
    NodeSpan s         = node < ast.spans.size() ? ast.spans[node] : NodeSpan{};
    const Token &a = ast.lex.tokens[s.first];
    const Token &b = ast.lex.tokens[s.last];
    return Pos{ a.line, a.bcol, b.eline, b.ecol };
}
