// The parser: tokens to a tree, by recursive descent.
//
// The node kinds and their fields mirror CPython's `ast` module, so a golden
// can be generated from CPython (tools/mkast.py) and the tree compared against
// what CPython built for the same source.
//
// Links are u32 indices into `nodes`, never pointers: the arrays reallocate as
// the parse grows. Node 0 is Nop, so an index of 0 reads as "nothing".
#pragma once

#include "lex.h"
#include "obj.h"

enum class Nd : u8 {
    Nop,

    // statements
    Module,
    FunctionDef,
    AsyncFunctionDef,
    ClassDef,
    Return,
    Delete,
    Assign,
    AugAssign,
    AnnAssign,
    For,
    AsyncFor,
    While,
    If,
    With,
    AsyncWith,
    Raise,
    Try,
    Assert,
    Import,
    ImportFrom,
    Global,
    Nonlocal,
    Expr,
    Pass,
    Break,
    Continue,

    // expressions
    BoolOp,
    NamedExpr,
    BinOp,
    UnaryOp,
    Lambda,
    IfExp,
    Dict,
    Set,
    ListComp,
    SetComp,
    DictComp,
    GeneratorExp,
    Await,
    Yield,
    YieldFrom,
    Compare,
    Call,
    JoinedStr,
    FormattedValue,
    Constant,
    Attribute,
    Subscript,
    Starred,
    Name,
    List,
    Tuple,
    Slice,

    // helpers
    CmpOp,     // one (operator, operand) of a comparison chain
    Comprehen, // one `for` clause of a comprehension
    ExceptHandler,
    Arguments,
    Arg,
    Keyword,
    Alias,
    WithItem,
};

// BoolOp flags.
enum class Bool : u8 { And, Or };

// FormattedValue flags: the conversion, spelled as CPython spells it.
enum : u8 { FCONV_NONE = 0, FCONV_STR = 's', FCONV_REPR = 'r', FCONV_ASCII = 'a' };

// Constant flags.
enum class Const : u8 { None, True, False, Ellipsis, Int, Float, Str, Bytes };

// Arguments flags.
enum : u8 {
    ARG_VARARG = 1 << 0, // a *args sits after the positional arguments
    ARG_KWARG  = 1 << 1, // a **kwargs sits after the keyword-only defaults
};

Str nd_name(Nd k);

// What each kind keeps, and in what order. `tok` names the token a node is at,
// and carries its text or its value; a, b, c and d are node indices or counts;
// kids is one contiguous run, sliced by those counts.
//
//   Module          kids: body
//   FunctionDef     tok: name  a: Arguments  b: #body  c: #decorators
//                   d: returns   kids: body ++ decorators
//   ClassDef        tok: name  a: #bases  b: #keywords  c: #body
//                   d: #decorators  kids: bases ++ keywords ++ body ++ decos
//   Return/Await/Yield/YieldFrom/Expr/Starred   a: value
//   Delete/Global/Nonlocal/Import               kids: targets, names or aliases
//   Assign          a: value  kids: targets
//   AugAssign       flags: Op  a: target  b: value
//   AnnAssign       a: target  b: annotation  c: value  flags bit 0: simple
//   For/AsyncFor    a: target  b: iter  c: #body  d: #orelse
//   While/If        a: test  b: #body  c: #orelse
//   With/AsyncWith  a: #items  b: #body  kids: items ++ body
//   Raise           a: exc  b: cause
//   Try             a: #body  b: #handlers  c: #orelse  d: #finalbody
//   Assert          a: test  b: msg
//   ImportFrom      tok: module (flags bit 0 when there is one)  a: level
//   BoolOp          flags: Bool   kids: values
//   NamedExpr/BinOp a: left/target  b: right/value  flags: Op for BinOp
//   UnaryOp         flags: Un  a: operand
//   Lambda          a: Arguments  b: body
//   IfExp           a: test  b: body  c: orelse
//   Dict            kids: key, value, key, value...  a key of 0 is `**value`
//   Set/List/Tuple  kids: elts
//   ListComp/SetComp/GeneratorExp   a: elt  kids: Comprehen...
//   DictComp        a: key  b: value  kids: Comprehen...
//   Compare         a: left  kids: CmpOp...
//   CmpOp           flags: Cmp  a: operand
//   Call            a: func  b: #args  kids: args ++ keywords
//   JoinedStr       kids: values -- Constant str and FormattedValue, in order
//   FormattedValue  a: value  b: format_spec, a JoinedStr, or 0 for none
//                   flags: CONV_*, the !s !r !a a field asked for
//   Constant        flags: Const  tok: the literal
//   Attribute       a: value  tok: the name
//   Subscript       a: value  b: slice
//   Name            tok: the identifier
//   Slice           a: lower  b: upper  c: step
//   Comprehen       a: target  b: iter  flags bit 0: async  kids: ifs
//   ExceptHandler   a: type  b: #body  tok: name (flags bit 0 when there is
//                   one)  kids: body
//   Arguments       a: #posonly  b: #args  c: #kwonly  d: #defaults
//                   flags: ARG_VARARG, ARG_KWARG
//                   kids: posonly ++ args ++ [vararg] ++ kwonly
//                         ++ kw_defaults ++ [kwarg] ++ defaults
//   Arg             tok: name  a: annotation
//   Keyword         tok: name (flags bit 0 when there is one)  a: value
//   Alias           tok: name  a: asname token + 1, or 0
//   WithItem        a: context  b: vars
struct Node {
    Nd kind  = Nd::Nop;
    u8 flags = 0;
    u16 pad  = 0;
    u32 tok  = 0;
    u32 a = 0, b = 0, c = 0, d = 0;
    u32 kid0 = 0, nkid = 0;
};

// The parser bounds its own recursion: it runs on the 128 KiB native stack.
constexpr u32 MAX_NEST = 100;

struct Ast {
    Lexer lex;
    Vec<Node> nodes;
    Vec<u32> kids;
    u32 root = 0;

    // False leaves a SyntaxError pending, with the line and column.
    bool parse(Str source);

    const Node &at(u32 i) const { return nodes[i]; }

    Str text(u32 node) const { return lex.text_of(lex.tokens[nodes[node].tok]); }
};

// The tree, one node per line, for --dump-ast.
bool ast_dump(Str source, String &out);
