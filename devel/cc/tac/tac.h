#ifndef TACKY_H
#define TACKY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "float128.h"

// Forward declarations
typedef struct Tac_Val Tac_Val;
typedef struct Tac_Instruction Tac_Instruction;
typedef struct Tac_Type Tac_Type;
typedef struct Tac_StaticInit Tac_StaticInit;
typedef struct Tac_TopLevel Tac_TopLevel;
typedef struct Tac_Param Tac_Param;
typedef struct Tac_StaticLocal Tac_StaticLocal;
typedef struct Tac_Member Tac_Member;

//
// Program: TopLevel* decls
//
typedef struct {
    Tac_TopLevel *decls; // Head of TopLevel linked list
} Tac_Program;

//
// A named frame-resident value: a parameter, an automatic local or a temporary.
//
typedef struct Tac_Param {
    struct Tac_Param *next; // Linked list
    char *name;
    Tac_Type *type; // NULL when unknown (hand-built TAC)
} Tac_Param;

//
// A block-scope static variable, stored per function.  Its storage is emitted
// inside the owning function's own module (after the code, before the end marker)
// as a module-local label, rather than as a separate top-level static variable.
//
typedef struct Tac_StaticLocal {
    struct Tac_StaticLocal *next; // Linked list
    char *name;
    Tac_Type *type;
    Tac_StaticInit *init_list; // optional; NULL ⇒ zero-initialized (bss)
    int alignment;             // bytes from _Alignas; 0 ⇒ the type's own
} Tac_StaticLocal;

//
// TopLevel: Function | StaticVariable | StaticConstant | Extern
//
typedef enum {
    TAC_TOPLEVEL_FUNCTION,
    TAC_TOPLEVEL_STATIC_VARIABLE,
    TAC_TOPLEVEL_STATIC_CONSTANT,
    TAC_TOPLEVEL_EXTERN
} Tac_TopLevelKind;

typedef struct Tac_TopLevel {
    struct Tac_TopLevel *next; // Linked list
    Tac_TopLevelKind kind;
    union {
        struct {
            char *name;
            bool global;
            bool variadic;
            bool noret;                     // True if declared/defined _Noreturn
            Tac_Type *type;                 // Function type (FUN_TYPE); NULL when unknown
            Tac_Param *params;              // Parameters, in order
            Tac_Param *locals;              // Automatic locals and temporaries; with params,
                                            // every frame-resident name of the body
            Tac_StaticLocal *static_locals; // Block-scope static variables, emitted
                                            // inside this function's module.
            Tac_Instruction *body;          // Linked list of instructions
        } function;
        struct {
            char *name;
            bool global;
            Tac_Type *type;
            Tac_StaticInit *init_list; // Linked list of initializers
            int alignment;             // bytes from _Alignas; 0 ⇒ the type's own
        } static_variable;
        struct {
            char *name;
            Tac_Type *type;
            Tac_StaticInit *init;
        } static_constant;
        struct {
            char *name;     // object or function referenced but not defined in this unit
            Tac_Type *type; // its declared type
        } extern_;
    } u;
} Tac_TopLevel;

//
// Instruction: Various kinds
//
typedef enum {
    TAC_INSTRUCTION_RETURN,
    TAC_INSTRUCTION_SIGN_EXTEND,
    TAC_INSTRUCTION_TRUNCATE,
    TAC_INSTRUCTION_ZERO_EXTEND,
    TAC_INSTRUCTION_DOUBLE_TO_INT,
    TAC_INSTRUCTION_DOUBLE_TO_UINT,
    TAC_INSTRUCTION_INT_TO_DOUBLE,
    TAC_INSTRUCTION_UINT_TO_DOUBLE,
    TAC_INSTRUCTION_FLOAT_TO_DOUBLE,
    TAC_INSTRUCTION_DOUBLE_TO_FLOAT,
    TAC_INSTRUCTION_INT_TO_FLOAT,
    TAC_INSTRUCTION_UINT_TO_FLOAT,
    TAC_INSTRUCTION_FLOAT_TO_INT,
    TAC_INSTRUCTION_FLOAT_TO_UINT,
    TAC_INSTRUCTION_LONG_DOUBLE_TO_INT,
    TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT,
    TAC_INSTRUCTION_INT_TO_LONG_DOUBLE,
    TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE,
    TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE,
    TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE,
    TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT,
    TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE,
    // Pointer-representation conversions (BESM-6 fat pointers).  A word pointer
    // (int*, etc.) and a byte/fat pointer (char*, void*) have different bit layouts:
    // a fat pointer carries a byte offset and a marker bit.  These share the {src,dst}
    // layout of the numeric conversions and are a plain copy on byte-addressed targets.
    TAC_INSTRUCTION_PTR_TO_CHAR_PTR, // word pointer  → char*/void* (fat)
    TAC_INSTRUCTION_CHAR_PTR_TO_PTR, // char*/void* (fat) → word pointer
    TAC_INSTRUCTION_UNARY,
    TAC_INSTRUCTION_BINARY,
    TAC_INSTRUCTION_COPY,
    TAC_INSTRUCTION_GET_ADDRESS,
    TAC_INSTRUCTION_GET_ADDRESS_BYTE,  // &char object → char*/void* fat pointer (offset_enc 0)
    TAC_INSTRUCTION_GET_ADDRESS_DECAY, // char-array/string decay → fat pointer (offset_enc 5)
    TAC_INSTRUCTION_LOAD,
    TAC_INSTRUCTION_LOAD_BYTE, // dereference a single byte through a fat pointer
    TAC_INSTRUCTION_STORE,
    TAC_INSTRUCTION_STORE_BYTE, // store a single byte through a fat pointer
    TAC_INSTRUCTION_ADD_PTR,
    TAC_INSTRUCTION_PTR_DIFF, // char*/void* difference → ptrdiff_t (long) byte count
    TAC_INSTRUCTION_COPY_TO_OFFSET,
    TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET, // sub-word packed char member (byte read-modify-write)
    TAC_INSTRUCTION_COPY_FROM_OFFSET,
    TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET, // sub-word packed char member (byte extract)
    TAC_INSTRUCTION_JUMP,
    TAC_INSTRUCTION_JUMP_IF_ZERO,
    TAC_INSTRUCTION_JUMP_IF_NOT_ZERO,
    TAC_INSTRUCTION_LABEL,
    TAC_INSTRUCTION_FUN_CALL,
    TAC_INSTRUCTION_FUN_CALL_NORETURN, // call to a _Noreturn function (shares u.fun_call)
    TAC_INSTRUCTION_ALLOCATE_LOCAL,
    TAC_INSTRUCTION_JUMP_TABLE // a multiway jump on an unsigned index (the coroutine dispatch)
} Tac_InstructionKind;

typedef enum {
    TAC_UNARY_COMPLEMENT,
    TAC_UNARY_NEGATE,
    TAC_UNARY_NOT,
    TAC_UNARY_NEGATE_UNSIGNED,     // negate, 48-bit modular (unsigned operand)
    TAC_UNARY_NEGATE_DOUBLE,       // negate, floating-point operand
    TAC_UNARY_COMPLEMENT_UNSIGNED, // complement, full 48-bit flip (unsigned operand)
    TAC_UNARY_SQRT_DOUBLE          // square root of a double: a call of the C library's
                                   // sqrt, on a target where it is one instruction
} Tac_UnaryOperator;

typedef enum {
    TAC_BINARY_ADD,
    TAC_BINARY_SUBTRACT,
    TAC_BINARY_MULTIPLY,
    TAC_BINARY_DIVIDE,
    TAC_BINARY_REMAINDER,
    TAC_BINARY_EQUAL,
    TAC_BINARY_NOT_EQUAL,
    TAC_BINARY_LESS_THAN,
    TAC_BINARY_LESS_OR_EQUAL,
    TAC_BINARY_GREATER_THAN,
    TAC_BINARY_GREATER_OR_EQUAL,
    TAC_BINARY_BITWISE_AND,
    TAC_BINARY_BITWISE_OR,
    TAC_BINARY_BITWISE_XOR,
    TAC_BINARY_LEFT_SHIFT,
    TAC_BINARY_RIGHT_SHIFT,
    TAC_BINARY_DIVIDE_UNSIGNED,
    TAC_BINARY_REMAINDER_UNSIGNED,
    TAC_BINARY_LESS_THAN_UNSIGNED,
    TAC_BINARY_LESS_OR_EQUAL_UNSIGNED,
    TAC_BINARY_GREATER_THAN_UNSIGNED,
    TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED,
    TAC_BINARY_RIGHT_SHIFT_LOGICAL,
    TAC_BINARY_ADD_UNSIGNED,
    TAC_BINARY_SUBTRACT_UNSIGNED,
    TAC_BINARY_MULTIPLY_UNSIGNED,
    TAC_BINARY_ADD_DOUBLE,             // add, floating-point operands
    TAC_BINARY_SUBTRACT_DOUBLE,        // subtract, floating-point operands
    TAC_BINARY_MULTIPLY_DOUBLE,        // multiply, floating-point operands
    TAC_BINARY_DIVIDE_DOUBLE,          // divide, floating-point operands
    TAC_BINARY_LESS_THAN_DOUBLE,       // <,  floating-point operands
    TAC_BINARY_LESS_OR_EQUAL_DOUBLE,   // <=, floating-point operands
    TAC_BINARY_GREATER_THAN_DOUBLE,    // >,  floating-point operands
    TAC_BINARY_GREATER_OR_EQUAL_DOUBLE // >=, floating-point operands
} Tac_BinaryOperator;

enum { TAC_DST_KIND_UNKNOWN = -1, TAC_DST_KIND_NO_FOLD = -2 };

typedef struct Tac_Instruction {
    struct Tac_Instruction *next; // Linked list
    Tac_InstructionKind kind;
    // Set on a memory access (LOAD/STORE/COPY/COPY_TO_OFFSET/COPY_FROM_OFFSET)
    // that touches a volatile-qualified object. The optimizer must preserve such
    // an access verbatim: never eliminate, duplicate, reorder, or propagate it.
    bool is_volatile;
    union {
        struct {
            Tac_Val *src;
        } return_;
        // The three integer-width conversions carry the destination's Tac_ConstKind
        // (or TAC_DST_KIND_UNKNOWN = -1 = "not supplied", or TAC_DST_KIND_NO_FOLD for a
        // destination with no constant kind, e.g. a 16-bit short: never folded) so the constant
        // folder can label a folded result with the conversion's true result type — a promotion
        // `unsigned char → int` and an explicit cast `unsigned char → unsigned int` both lower to
        // the same ZERO_EXTEND, and only the destination kind distinguishes them. The shared
        // {src,dst,dst_kind} prefix lets the folder read it through `u.sign_extend`.
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind;
        } sign_extend;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind;
        } truncate;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind;
        } zero_extend;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind; // as for sign_extend
        } double_to_int;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind; // as for sign_extend
        } double_to_uint;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } int_to_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } uint_to_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } float_to_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } double_to_float;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } int_to_float;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } uint_to_float;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind; // as for sign_extend
        } float_to_int;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind; // as for sign_extend
        } float_to_uint;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind; // as for sign_extend
        } long_double_to_int;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
            int dst_kind; // as for sign_extend
        } long_double_to_uint;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } int_to_long_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } uint_to_long_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } long_double_to_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } double_to_long_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } long_double_to_float;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } float_to_long_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } ptr_to_char_ptr;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } char_ptr_to_ptr;
        struct {
            Tac_UnaryOperator op;
            Tac_Val *src;
            Tac_Val *dst;
        } unary;
        struct {
            Tac_BinaryOperator op;
            Tac_Val *src1;
            Tac_Val *src2;
            Tac_Val *dst;
        } binary;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } copy;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } get_address; // also GET_ADDRESS_BYTE / GET_ADDRESS_DECAY
        struct {
            Tac_Val *src_ptr;
            Tac_Val *dst;
        } load; // also LOAD_BYTE
        struct {
            Tac_Val *src;
            Tac_Val *dst_ptr;
        } store; // also STORE_BYTE
        struct {
            Tac_Val *ptr;
            Tac_Val *index;
            int scale;
            Tac_Val *dst;
        } add_ptr;
        struct {
            Tac_Val *ptr_a; // minuend   (p)
            Tac_Val *ptr_b; // subtrahend (q)
            Tac_Val *dst;   // ptrdiff_t byte count
        } ptr_diff;
        struct {
            Tac_Val *src;
            char *dst;
            int offset;
        } copy_to_offset; // also COPY_BYTE_TO_OFFSET
        struct {
            char *src;
            int offset;
            Tac_Val *dst;
        } copy_from_offset; // also COPY_BYTE_FROM_OFFSET
        struct {
            char *target;
        } jump;
        struct {
            Tac_Val *condition;
            char *target;
        } jump_if_zero;
        struct {
            Tac_Val *condition;
            char *target;
        } jump_if_not_zero;
        struct {
            char *name;
        } label;
        struct {
            Tac_Val *index; // an unsigned int
            char **targets; // index i jumps to targets[i], for i < count
            int count;
            char *default_target; // any other index
        } jump_table;
        struct {
            char *fun_name;
            // How to read fun_name.  False: it names the callee itself — a direct call to
            // that function.  True: it names an *object* holding the callee's address — a
            // call through a function pointer, whether that object is a parameter, a local,
            // a compiler temporary or a module-level variable.  The name alone cannot say
            // which (a global `f` may be either), and a backend that guesses from frame
            // residency calls the pointer's own storage.  FUN_CALL_NORETURN is always direct.
            bool indirect;
            Tac_Val *args; // Linked list of values
            Tac_Val *dst;
            Tac_Type *fun_type; // Callee's type (FUN_TYPE); NULL when unknown
        } fun_call;
        struct {
            char *name;    // frame-resident local aggregate name
            int size;      // slot size in target bytes
            int alignment; // slot alignment in target bytes
        } allocate_local;
    } u;
} Tac_Instruction;

//
// Val: Constant | Var
//
typedef enum { TAC_VAL_CONSTANT, TAC_VAL_VAR } Tac_ValKind;

typedef struct Tac_Val {
    struct Tac_Val *next; // Linked list
    Tac_ValKind kind;
    union {
        struct Tac_Const *constant;
        char *var_name;
    } u;
} Tac_Val;

//
// Const: Various constant types
//
typedef enum {
    TAC_CONST_INT,
    TAC_CONST_LONG,
    TAC_CONST_LONG_LONG,
    TAC_CONST_UINT,
    TAC_CONST_ULONG,
    TAC_CONST_ULONG_LONG,
    TAC_CONST_FLOAT,
    TAC_CONST_DOUBLE,
    TAC_CONST_LONG_DOUBLE,
    TAC_CONST_SCHAR,
    TAC_CONST_UCHAR
} Tac_ConstKind;

typedef struct Tac_Const {
    Tac_ConstKind kind;
    union {
        int64_t int_val;
        long long_val;
        long long long_long_val;
        uint64_t uint_val;
        unsigned long ulong_val;
        unsigned long long ulong_long_val;
        double float_val;
        double double_val;
        Float128 long_double_val; // binary128 bits
        int char_val;
        unsigned char uchar_val;
    } u;
} Tac_Const;

//
// Type: Various type kinds
//
typedef enum {
    TAC_TYPE_SCHAR,
    TAC_TYPE_UCHAR,
    TAC_TYPE_SHORT,
    TAC_TYPE_INT,
    TAC_TYPE_LONG,
    TAC_TYPE_LONG_LONG,
    TAC_TYPE_USHORT,
    TAC_TYPE_UINT,
    TAC_TYPE_ULONG,
    TAC_TYPE_ULONG_LONG,
    TAC_TYPE_FLOAT,
    TAC_TYPE_DOUBLE,
    TAC_TYPE_LONG_DOUBLE,
    TAC_TYPE_VOID,
    TAC_TYPE_FUN_TYPE,
    TAC_TYPE_POINTER,
    TAC_TYPE_ARRAY,
    TAC_TYPE_STRUCTURE
} Tac_TypeKind;

typedef struct Tac_Type {
    struct Tac_Type *next; // Linked list
    Tac_TypeKind kind;
    union {
        struct {
            Tac_Type *param_types; // Linked list of types
            Tac_Type *ret_type;
            bool variadic;
        } fun_type;
        struct {
            Tac_Type *target_type;
        } pointer;
        struct {
            Tac_Type *elem_type;
            int size;
        } array;
        struct {
            char *tag;
            int size;      // in bytes; 0 when incomplete
            int alignment; // in bytes
            bool is_union;
            Tac_Member *members; // by offset; NULL when not expanded (behind a pointer)
        } structure;
    } u;
} Tac_Type;

//
// A struct/union member: its byte offset and type.
//
typedef struct Tac_Member {
    struct Tac_Member *next; // Linked list
    char *name;
    int offset;
    Tac_Type *type;
} Tac_Member;

//
// StaticInit: Various initialization kinds
//
typedef enum {
    TAC_STATIC_INIT_I8,
    TAC_STATIC_INIT_I16,
    TAC_STATIC_INIT_I32,
    TAC_STATIC_INIT_I64,
    TAC_STATIC_INIT_U8,
    TAC_STATIC_INIT_U16,
    TAC_STATIC_INIT_U32,
    TAC_STATIC_INIT_U64,
    TAC_STATIC_INIT_FLOAT,
    TAC_STATIC_INIT_DOUBLE,
    TAC_STATIC_INIT_LONG_DOUBLE,
    TAC_STATIC_INIT_ZERO,
    TAC_STATIC_INIT_STRING,
    TAC_STATIC_INIT_POINTER,
    TAC_STATIC_INIT_FAT_POINTER
} Tac_StaticInitKind;

typedef struct Tac_StaticInit {
    struct Tac_StaticInit *next; // Linked list
    Tac_StaticInitKind kind;
    union {
        int8_t char_val;          // INIT_I8
        int16_t short_val;        // INIT_I16
        int32_t int_val;          // INIT_I32
        int64_t long_val;         // INIT_I64
        uint8_t uchar_val;        // INIT_U8
        uint16_t ushort_val;      // INIT_U16
        uint32_t uint_val;        // INIT_U32
        uint64_t ulong_val;       // INIT_U64
        double float_val;         // INIT_FLOAT
        double double_val;        // INIT_DOUBLE
        Float128 long_double_val; // INIT_LONG_DOUBLE, binary128 bits
        int zero_bytes;           // INIT_ZERO

        // INIT_STRING
        struct {
            char *val;  // decoded bytes; NUL-terminated for printing, but see len
            size_t len; // byte count — a decoded literal may hold embedded NULs
            bool null_terminated;
        } string;

        // INIT_POINTER, INIT_FAT_POINTER
        struct {
            char *name;
            int byte_offset; // Total byte offset from symbol.  On BESM-6:
                             // INIT_POINTER: multiple of 6
                             // INIT_FAT_POINTER: word*6 + byte_from_MSB
        } pointer;
    } u;
} Tac_StaticInit;

//
// Allocate
//
Tac_Val *tac_new_val(Tac_ValKind kind);
Tac_Instruction *tac_new_instruction(Tac_InstructionKind kind);
Tac_Type *tac_new_type(Tac_TypeKind kind);
Tac_Const *tac_new_const(Tac_ConstKind kind);
Tac_Param *tac_new_param(void);
Tac_Member *tac_new_member(void);
Tac_StaticLocal *tac_new_static_local(void);
Tac_TopLevel *tac_new_toplevel(Tac_TopLevelKind kind);
Tac_StaticInit *tac_new_static_init(Tac_StaticInitKind kind);
Tac_Program *tac_new_program(void);
// Deep copy of a type, including its ->next chain.
Tac_Type *tac_clone_type(const Tac_Type *type);

//
// Deallocate
//
void tac_free_program(Tac_Program *program);
void tac_free_const(Tac_Const *constant);
void tac_free_val(Tac_Val *val);
void tac_free_instruction(Tac_Instruction *instr);
void tac_free_type(Tac_Type *type);
void tac_free_param(Tac_Param *param);
void tac_free_member(Tac_Member *member);
void tac_free_static_local(Tac_StaticLocal *sl);
void tac_free_static_init(Tac_StaticInit *init);
void tac_free_toplevel(Tac_TopLevel *toplevel);

//
// Print
//
// Platform-independent "%a"-style hex float.  glibc and macOS disagree only on
// subnormals (glibc: 0x0.0..1p-1022, macOS: 0x1p-1074); this normalizes via frexp so the
// significand is always in [1,2) — a normal value both libcs format identically.
void tac_format_hex_double(char *out, size_t outsz, double v);

// Render len bytes of a decoded string literal as printable text (C escapes for
// non-printables), for the TAC dumps.  Returns an xalloc'd string; caller xfree()s it.
char *tac_escape_string_bytes(const char *s, size_t len);
void tac_print_const(FILE *fd, const Tac_Const *constant, int depth);
void tac_print_val(FILE *fd, const Tac_Val *val, int depth);
void tac_print_type(FILE *fd, const Tac_Type *type, int depth);
// One-line C-like spelling of a type, e.g. "*int", "[4]uchar", "fn(int, ...) -> void".
// Returns an xalloc'd string; caller xfree()s it.
char *tac_type_str(const Tac_Type *type);
void tac_print_param(FILE *fd, const Tac_Param *param, int depth);
void tac_print_static_init(FILE *fd, const Tac_StaticInit *init, int depth);
void tac_print_instruction(FILE *fd, const Tac_Instruction *instr, int depth);
void tac_print_toplevel(FILE *fd, const Tac_TopLevel *toplevel, int depth);
void tac_print_program(FILE *fd, const Tac_Program *program);

//
// Walk
//
// Call fn for every name an instruction mentions: each variable operand, the bare
// aggregate names of COPY_*_OFFSET and ALLOCATE_LOCAL, and a call's fun_name (the
// callee, or the pointer variable of an indirect call).  Labels are not names.
typedef void (*Tac_NameVisitor)(const char *name, void *arg);
void tac_visit_names(const Tac_Instruction *in, Tac_NameVisitor fn, void *arg);
// True when static variable `tl` emits no storage: it is a tentative definition and
// `program` has a real definition of the name, or an earlier tentative one.
bool tac_static_superseded(const Tac_TopLevel *program, const Tac_TopLevel *tl);

//
// AAPCS64 argument classes (tac_abi.c): in general registers, by reference to a copy
// (an aggregate over 16 bytes), or in FP registers as `count` (1-4) elements of `esize`
// (4, 8 or 16) bytes each, encoded esize * 8 + count — a float, double or long double
// and a homogeneous float aggregate alike.  The value of __builtin_va_class.
//
enum { TAC_AAPCS64_GENERAL = 0, TAC_AAPCS64_BY_REF = 1 };
int tac_aapcs64_class(const Tac_Type *t);
// The element count of `t` as a homogeneous float aggregate (a lone FP scalar is one),
// with the element size in *esize; 0 when it is none.
int tac_aapcs64_hfa(const Tac_Type *t, int *esize);
// The same under Apple's arm64 ABI, where long double is double.
int tac_apple64_hfa(const Tac_Type *t, int *esize);
// How a variadic argument goes under Apple's arm64 ABI: TAC_AAPCS64_BY_REF for an
// aggregate over 16 bytes that is no homogeneous float aggregate, else
// TAC_AAPCS64_GENERAL, in 8-byte stack slots (an HFA too).  The value of
// __builtin_va_class on aarch64-darwin.
int tac_apple64_class(const Tac_Type *t);
//
// AAPCS-VFP (32-bit) argument classes (tac_abi.c): in core registers and the stack, or
// in VFP registers as `count` (1-4) elements of `esize` (4 or 8) bytes, encoded
// esize * 8 + count — a float or double (long double is one) and a homogeneous float
// aggregate alike.  Under the base standard (variadic) everything is TAC_AAPCS32_CORE.
//
enum { TAC_AAPCS32_CORE = 0 };
int tac_aapcs32_class(const Tac_Type *t);
//
// MSP430 EABI argument classes, as GCC implements it (tac_abi.c): a structure or union
// goes by reference, a scalar by value.  The value of __builtin_va_class.
//
enum { TAC_MSP430_VALUE = 0, TAC_MSP430_BY_REF = 1 };
int tac_msp430_class(const Tac_Type *t);
//
// WebAssembly C ABI, as clang's wasm32 (tac_abi.c).  The scalar a value of type `t`
// travels as: `t` itself for a scalar, the one scalar of an aggregate holding exactly
// one (a lone bit-field's as the unsigned integer of the aggregate's size), else NULL:
// an empty aggregate travels not at all, any other by reference to a copy.  The class
// of a variadic argument is the value of __builtin_va_class.
//
enum { TAC_WASM32_VALUE = 0, TAC_WASM32_BY_REF = 1 };
const Tac_Type *tac_wasm32_scalar(const Tac_Type *t);
bool tac_wasm32_empty(const Tac_Type *t);
int tac_wasm32_class(const Tac_Type *t);
//
// System V AMD64 argument classes (tac_abi.c): in memory (over 16 bytes, or with an
// eightbyte that merges to MEMORY), X87 (a long double, or a struct of one: returned in
// st(0), passed in memory), or the class of each eightbyte, INTEGER or SSE, encoded
// class0 | class1 << 2 (class1 0 for a type of one eightbyte).  The value of
// __builtin_va_class.
//
enum { TAC_SYSV64_MEMORY = 0, TAC_SYSV64_INTEGER = 1, TAC_SYSV64_SSE = 2, TAC_SYSV64_X87 = 3 };
int tac_sysv64_class(const Tac_Type *t);
// Eightbyte `i` (0 or 1) of class `cls`, or 0 when there is none.
#define TAC_SYSV64_EIGHTBYTE(cls, i) ((cls) >> 2 * (i) & 3)

//
// Verify
//
// Byte sizes on the target: of each scalar kind (indexed by Tac_TypeKind, up to
// TAC_TYPE_LONG_DOUBLE) and of a pointer.
typedef struct {
    int scalar[TAC_TYPE_VOID];
    int pointer;
} Tac_Layout;
// The type of a name that is not frame-resident (a global, function, string constant),
// or NULL when unknown.
typedef const Tac_Type *(*Tac_GlobalType)(const char *name, void *arg);
// Check one function: every name has a type and operand types agree with the operator.
// Prints each problem to `err` (when not NULL) and returns how many were found.
int tac_verify_function(const Tac_TopLevel *fn, const Tac_Layout *layout, Tac_GlobalType global,
                        void *arg, FILE *err);
// Check every function of a program chain, resolving globals against its toplevels.
int tac_verify_program(const Tac_TopLevel *program, const Tac_Layout *layout, FILE *err);
// The YAML spelling of an instruction kind, e.g. "add_ptr".
const char *tac_instruction_name(Tac_InstructionKind kind);

//
// Graphviz (instruction-level sketch)
//
void tac_export_dot(FILE *fd, const Tac_TopLevel *toplevel);

//
// Compare
//
bool tac_compare_const(const Tac_Const *a, const Tac_Const *b);
bool tac_compare_val(const Tac_Val *a, const Tac_Val *b);
bool tac_compare_type(const Tac_Type *a, const Tac_Type *b);
bool tac_compare_param(const Tac_Param *a, const Tac_Param *b);
bool tac_compare_static_init(const Tac_StaticInit *a, const Tac_StaticInit *b);
bool tac_compare_instruction(const Tac_Instruction *a, const Tac_Instruction *b);
bool tac_compare_toplevel(const Tac_TopLevel *a, const Tac_TopLevel *b);
bool tac_compare_program(const Tac_Program *a, const Tac_Program *b);

//
// Binary export (wio stream)
//
typedef struct _wfile WFILE;

// A stream is the magic word, the toplevels, then the end marker.
void tac_export_begin_stream(WFILE *out);
void tac_export_toplevel(WFILE *out, const Tac_TopLevel *tl);
void tac_export_end_stream(WFILE *out);
void tac_export_program(WFILE *out, const Tac_Program *prog);

//
// Binary import (wio stream)
//
// Read and check the magic word; false if this is not a TAC stream of this version.
bool tac_import_begin_stream(WFILE *in);
Tac_TopLevel *tac_import_toplevel(WFILE *in);
Tac_Program *tac_import_program(WFILE *in);

//
// YAML export
//
// When false, the YAML omits the type annotations (symbol types, callee types,
// function types), leaving the instruction-level view.  Default true.
extern bool tac_yaml_types;
void tac_export_yaml(FILE *fd, const Tac_TopLevel *tl);
void tac_export_yaml_instruction_list(FILE *fd, const Tac_Instruction *instr, int level);

#ifdef __cplusplus
}
#endif

#endif // TACKY_H
