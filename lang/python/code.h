// The bytecode: the opcode table, the instruction and the code object.
//
// An operand is a whole u32, so there is no EXTENDED_ARG, and a jump operand is
// an absolute instruction index, so patching a forward jump is one store.
//
// The stack layouts the trickier opcodes expect are written down beside them:
// phase 6 and phase 7 implement what is stated here, and the compiler already
// emits it.
#pragma once

#include "obj.h"

// What an operand means. The disassembler needs nothing else to print a hint.
enum class Arg : u8 {
    None,  // no operand
    Num,   // a count, a depth or an index into nothing
    Const, // consts[]
    Name,  // names[]
    Local, // varnames[]
    Deref, // cellvars[] ++ freevars[]
    Jump,  // an instruction index
    Un,    // Un
    Bin,   // Op
    Cmp,   // Cmp
    Flags, // MF_*, or CX_*
};

// The whole instruction set in one table: the enum, the names and the operand
// kinds all come from here, so they cannot drift apart. No // comments inside,
// line splicing happening before comments are removed.
#define BC_LIST(X)            \
    X(Nop, None)              \
                              \
    X(PopTop, None)           \
    X(DupTop, None)           \
    X(DupTop2, None)          \
    X(RotTwo, None)           \
    X(RotThree, None)         \
    X(RotFour, None)          \
                              \
    X(LoadConst, Const)       \
    X(LoadName, Name)         \
    X(StoreName, Name)        \
    X(DeleteName, Name)       \
    X(LoadFast, Local)        \
    X(StoreFast, Local)       \
    X(DeleteFast, Local)      \
    X(LoadGlobal, Name)       \
    X(StoreGlobal, Name)      \
    X(DeleteGlobal, Name)     \
    X(LoadDeref, Deref)       \
    X(StoreDeref, Deref)      \
    X(DeleteDeref, Deref)     \
    X(LoadClosure, Deref)     \
                              \
    X(LoadAttr, Name)         \
    X(StoreAttr, Name)        \
    X(DeleteAttr, Name)       \
    X(LoadSubscr, None)       \
    X(StoreSubscr, None)      \
    X(DeleteSubscr, None)     \
                              \
    X(UnaryOp, Un)            \
    X(BinaryOp, Bin)          \
    X(InplaceOp, Bin)         \
    X(CompareOp, Cmp)         \
                              \
    X(Jump, Jump)             \
    X(PopJumpIfFalse, Jump)   \
    X(PopJumpIfTrue, Jump)    \
    X(JumpIfFalseOrPop, Jump) \
    X(JumpIfTrueOrPop, Jump)  \
    X(GetIter, None)          \
    X(ForIter, Jump)          \
                              \
    X(BuildTuple, Num)        \
    X(BuildList, Num)         \
    X(BuildSet, Num)          \
    X(BuildMap, Num)          \
    X(BuildSlice, Num)        \
    X(BuildString, Num)       \
    X(FormatValue, Flags)     \
    X(ListAppend, Num)        \
    X(SetAdd, Num)            \
    X(MapAdd, Num)            \
    X(ListExtend, Num)        \
    X(SetUpdate, Num)         \
    X(DictUpdate, Num)        \
    X(DictMerge, Num)         \
    X(ListToTuple, None)      \
                              \
    X(UnpackSequence, Num)    \
    X(UnpackEx, Num)          \
                              \
    X(Call, Num)              \
    X(CallKw, Num)            \
    X(CallEx, Flags)          \
    X(MakeFunction, Flags)    \
    X(LoadBuildClass, None)   \
                              \
    X(ImportName, Name)       \
    X(ImportFrom, Name)       \
    X(ImportStar, None)       \
                              \
    X(Return, None)           \
    X(PrintExpr, None)        \
    X(YieldValue, None)       \
    X(YieldFrom, None)        \
    X(Raise, Num)             \
                              \
    X(SetupFinally, Jump)     \
    X(SetupWith, Jump)        \
    X(PopBlock, None)         \
    X(PushExcInfo, None)      \
    X(PopExcept, None)        \
    X(CheckExcMatch, None)    \
    X(Reraise, Num)           \
    X(BeforeWith, None)       \
    X(WithExceptStart, None)  \
    X(LoadAssertionError, None)

enum class Bc : u8 {
#define BC_ENUM(n, a) n,
    BC_LIST(BC_ENUM)
#undef BC_ENUM
};

Str bc_name(Bc op);
Arg bc_arg(Bc op);

// What the ones that are not obvious do.
//
//   RotTwo/Three/Four   lift the top value down two, three or four places
//   ForIter t           step the iterator on top and push the item; on the end
//                       pop the iterator and jump to t
//   ListAppend d        pop a value and add it to the container d below the
//                       new top; SetAdd the same, MapAdd pops value then key
//   ListExtend d        the same, but the popped value is iterated into it
//   DictMerge d         DictUpdate for a call's keywords: a key already there
//                       is a TypeError, since two ** cannot name one parameter
//   UnpackEx n          n is (before) | (after << 16); the middle becomes a
//                       list, so before + 1 + after values are pushed
//   CallKw n            n values below a tuple of the trailing names
//   CallEx f            a tuple of positional arguments, and a mapping over it
//                       when CX_KWARGS; the callable is under both
//   MakeFunction f      a code object on top, preceded by whatever MF_* asks
//   SetupFinally t      push a handler: on an exception the value stack is cut
//                       back to the depth here and the exception pushed, then
//                       control goes to t
//   SetupWith t         the same, but cut back to one below the depth here, so
//                       the manager's __exit__ survives into the handler
//   PushExcInfo         at a handler, slide what the frame was handling under
//                       the new exception
//   CheckExcMatch       pop a type and the copy of the exception under it,
//                       and push whether they match; the original stays below
//   Reraise n           re-raise the exception on top; n is 1 when a saved
//                       exc-info sits under it and has to be restored first
//   BeforeWith          pop the manager, push its __exit__ and then __enter__()
//   WithExceptStart     with [exit, exc], call exit(type, exc, tb) and push it
//   PrintExpr           pop a value and, unless it is None, print its repr.
//                       This is what a statement is worth in Single mode
//   BuildString n       join the n strings on top into one
//   FormatValue f       format the value on top, the spec above it when
//                       FV_SPEC; FV_CONV is the !s !r !a to apply first

// MakeFunction's operand.
enum : u32 {
    MF_DEFAULTS   = 1 << 0, // a tuple of positional defaults
    MF_KWDEFAULTS = 1 << 1, // a dict of keyword-only defaults
    MF_CLOSURE    = 1 << 2, // a tuple of cells, in freevars order
};

// FormatValue's operand: a conversion in the low octet, and whether a format
// spec is on the stack above the value.
enum : u32 {
    FV_CONV = 0xff, // 0, or 's', 'r' or 'a'
    FV_SPEC = 1 << 8,
};

// CallEx's operand.
enum : u32 {
    CX_KWARGS = 1 << 0, // a mapping of keyword arguments above the tuple
};

struct Instr {
    Bc op   = Bc::Nop;
    u32 arg = 0;
};

// Run-length: the line every instruction from `at` on was compiled from.
struct LineEntry {
    u32 at;
    u32 line;
};

// Code object flags.
enum : u32 {
    CO_VARARGS   = 1 << 0, // the last positional parameter is *args
    CO_VARKW     = 1 << 1, // the last parameter is **kwargs
    CO_GENERATOR = 1 << 2, // the body yields
    CO_NEWLOCALS = 1 << 3, // a function body: a bare name is a fast local
    CO_NESTED    = 1 << 4, // has free variables
};

// What a source is compiled as, which is compile()'s third argument. Exec is
// a module body. Eval is one expression the code returns. Single is a module
// body that prints what each statement was worth, which is the REPL's.
enum class CompileMode : u8 { Exec, Eval, Single };

// Every name array holds StrObj values, so one trace covers them all.
struct CodeObj : Obj {
    Vec<Instr> code;
    Vec<Value> consts;
    Vec<Value> names;    // globals, attributes, imports
    Vec<Value> varnames; // parameters first, then the other fast locals
    Vec<Value> cellvars; // locals a nested scope captures
    Vec<Value> freevars; // captured from an enclosing scope
    Vec<LineEntry> lines;
    Value name;
    Value qualname; // the dotted path through the enclosing scopes
    Value doc;      // the body's first string literal, or Nil
    Value filename;
    u32 flags     = 0;
    u32 argcount  = 0; // positional, posonly included
    u32 posonly   = 0;
    u32 kwonly    = 0;
    u32 stacksize = 0;
    u32 nblocks   = 0; // deepest SetupFinally nesting, for the frame's blocks
    u32 firstline = 0;
};

extern const Type code_type;

CodeObj *code_new(Value name, Value filename, u32 firstline);

// The locals a frame over this code has: the fast slots, then the cells.
usize code_nlocals(const CodeObj *c);

inline bool is_code(Value v)
{
    return v.is_obj() && v.obj()->type == &code_type;
}

inline CodeObj *code_of(Value v)
{
    return static_cast<CodeObj *>(v.obj());
}

// The line instruction `pc` was compiled from, or firstline when unknown.
u32 code_line(const CodeObj *c, u32 pc);

// The listing, this code object and every code object in its constants.
bool code_dis(const CodeObj *c, String &out);
