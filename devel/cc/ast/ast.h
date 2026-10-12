#ifndef AST_H
#define AST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "float128.h"
#include "srcloc.h"

/* Forward declarations for recursive types */
typedef struct Type Type;
typedef struct TypeQualifier TypeQualifier;
typedef struct Field Field;
typedef struct Enumerator Enumerator;
typedef struct Param Param;
typedef struct Declaration Declaration;
typedef struct DeclSpec DeclSpec;
typedef struct FunctionSpec FunctionSpec;
typedef struct AlignmentSpec AlignmentSpec;
typedef struct InitDeclarator InitDeclarator;
typedef struct Initializer Initializer;
typedef struct InitItem InitItem;
typedef struct Designator Designator;
typedef struct Expr Expr;
typedef struct Literal Literal;
typedef struct GenericAssoc GenericAssoc;
typedef struct Stmt Stmt;
typedef struct DeclOrStmt DeclOrStmt;
typedef struct ForInit ForInit;
typedef struct Program Program;
typedef struct ExternalDecl ExternalDecl;

/* Identifier */
typedef char *Ident;

/* Types */
typedef enum {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_SCHAR,
    TYPE_UCHAR,
    TYPE_SHORT,
    TYPE_USHORT,
    TYPE_INT,
    TYPE_UINT,
    TYPE_LONG,
    TYPE_ULONG,
    TYPE_LONG_LONG,
    TYPE_ULONG_LONG,
    TYPE_SIGNED,   // Internal for parser only
    TYPE_UNSIGNED, // Internal for parser only
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LONG_DOUBLE,
    TYPE_COMPLEX,
    TYPE_IMAGINARY,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_TYPEDEF_NAME,
    TYPE_ATOMIC
} TypeKind;

struct Type {
    TypeKind kind;
    union {
        struct {
            Type *base;
        } complex; /* for Complex, Imaginary */
        struct {
            Type *target;
            TypeQualifier *qualifiers;
        } pointer;
        struct {
            Type *element;
            Expr *size;
            TypeQualifier *qualifiers;
            bool is_static;
        } array;
        struct {
            Type *return_type;
            Param *params;
            bool variadic;
        } function;
        struct {
            Ident name;
            Field *fields;
            int cached_size;  /* bytes; 0 = not resolved (set by validate_type while tag is live) */
            int cached_align; /* bytes; 0 = not resolved */
            const struct StructDef *cached_def; /* the tag's definition; NULL = not resolved */
            Type *frame_yield;  /* _Coro_frame(Y, T): Y (void included); NULL for any other */
            Type *frame_result; /* _Coro_frame(Y, T): T */
        } struct_t;             /* optional name */
        struct {
            Ident name;
            Enumerator *enumerators;
        } enum_t;
        struct {
            Ident name;
        } typedef_name;
        struct {
            Type *base;
        } atomic;
    } u;
    TypeQualifier *qualifiers; /* attributes */
};

typedef enum {
    TYPE_QUALIFIER_CONST,
    TYPE_QUALIFIER_RESTRICT,
    TYPE_QUALIFIER_VOLATILE,
    TYPE_QUALIFIER_ATOMIC
} TypeQualifierKind;

struct TypeQualifier {
    TypeQualifier *next; /* linked list */
    TypeQualifierKind kind;
};

typedef enum { FIELD_MEMBER, FIELD_STATIC_ASSERT } FieldKind;

// Where a bit-field member lies, set by typecheck from the struct layout.  Its bits are
// read by loading the storage unit that holds them: unit_size bytes at the member's
// offset, as one unsigned integer of that size, or byte by byte (bytewise) when no
// integer of that size can be loaded there -- the span is not 1, 2, 4 or 8 bytes, or not
// aligned for one, or would reach past the end of the struct.  `pos` numbers bits in that
// integer's value from its least significant one, so it already reflects byte order.
typedef struct {
    int width;     // in bits; 0 for an ordinary member
    int pos;       // of the field's least significant bit within the unit's value
    int unit_size; // bytes
    bool bytewise; // load and store the unit byte by byte
} BitField;

struct Field {
    Field *next; /* linked list */
    FieldKind kind;
    union {
        struct {
            Type *type;
            Ident name;                /* optional */
            Expr *bitfield;            /* optional */
            AlignmentSpec *align_spec; /* optional: _Alignas */
        } member;
        struct {
            Expr *condition;
            char *message; /* optional (always set in C11, guarded for safety) */
        } static_assrt;    /* typo matches existing Declaration convention */
    } u;
};

struct Enumerator {
    Enumerator *next; /* linked list */
    Ident name;
    Expr *value; /* optional */
};

struct Param {
    Param *next; /* linked list */
    SrcLoc loc;  /* source position, for diagnostics */
    Ident name;  /* optional */
    Type *type;
    DeclSpec *specifiers; /* optional */
};

/* Declarations */
typedef enum { DECL_VAR, DECL_STATIC_ASSERT, DECL_EMPTY } DeclarationKind;

struct Declaration {
    Declaration *next; /* linked list for declaration_list */
    SrcLoc loc;        /* source position, for diagnostics */
    DeclarationKind kind;
    union {
        struct {
            DeclSpec *specifiers;
            InitDeclarator *declarators;
        } var;
        struct {
            Expr *condition;
            char *message;
        } static_assrt;
        struct {
            DeclSpec *specifiers;
            Type *type;
        } empty;
    } u;
};

typedef enum {
    STORAGE_CLASS_NONE,
    STORAGE_CLASS_TYPEDEF,
    STORAGE_CLASS_EXTERN,
    STORAGE_CLASS_STATIC,
    STORAGE_CLASS_THREAD_LOCAL,
    STORAGE_CLASS_AUTO,
    STORAGE_CLASS_REGISTER
} StorageClass;

struct DeclSpec {
    TypeQualifier *qualifiers; // const, volatile, restrict, _Atomic
    StorageClass storage;      // extern, static, auto, register ...
    FunctionSpec *func_specs;  // inline, _Noreturn
    AlignmentSpec *align_spec; // _Alignas
};

typedef enum {
    FUNC_SPEC_INLINE,
    FUNC_SPEC_NORETURN,
    FUNC_SPEC_CORO /* vcc extension: _Coro(Y) */
} FunctionSpecKind;

struct FunctionSpec {
    FunctionSpec *next; /* linked list */
    FunctionSpecKind kind;
    Type *yield_type; /* FUNC_SPEC_CORO: Y, void included; else NULL */
};

typedef enum { ALIGN_SPEC_TYPE, ALIGN_SPEC_EXPR } AlignmentSpecKind;

struct AlignmentSpec {
    AlignmentSpecKind kind;
    union {
        Type *type;
        Expr *expr;
    } u;
};

struct InitDeclarator {
    InitDeclarator *next; /* linked list */
    SrcLoc loc;           /* source position, for diagnostics */
    Type *type;
    Ident name;
    Initializer *init; /* optional */
};

typedef enum { INITIALIZER_SINGLE, INITIALIZER_COMPOUND } InitializerKind;

struct Initializer {
    InitializerKind kind;
    SrcLoc loc; /* source position, for diagnostics */
    union {
        Expr *expr;
        InitItem *items;
    } u;
    Type *type; // for typecheck
};

struct InitItem {
    InitItem *next; /* linked list */
    Designator *designators;
    Initializer *init;
    int offset;  /* byte offset of this item within the enclosing struct; set by typecheck (0
                    otherwise) */
    BitField bf; /* a bit-field member's place in the unit at offset; set by typecheck */
};

typedef enum { DESIGNATOR_ARRAY, DESIGNATOR_FIELD } DesignatorKind;

struct Designator {
    Designator *next; /* linked list */
    DesignatorKind kind;
    union {
        Expr *expr; /* array index */
        Ident name; /* field name */
    } u;
};

/* Expressions */
typedef enum {
    EXPR_LITERAL,
    EXPR_VAR,
    EXPR_UNARY_OP,
    EXPR_BINARY_OP,
    EXPR_SUBSCRIPT,
    EXPR_ASSIGN,
    EXPR_COND,
    EXPR_CAST,
    EXPR_CALL,
    EXPR_COMPOUND,
    EXPR_FIELD_ACCESS,
    EXPR_PTR_ACCESS,
    EXPR_POST_INC,
    EXPR_POST_DEC,
    EXPR_SIZEOF_EXPR,
    EXPR_SIZEOF_TYPE,
    EXPR_ALIGNOF,
    EXPR_GENERIC,
    EXPR_VA_CLASS, // __builtin_va_class(type): the target's argument class, for va_arg
    EXPR_YIELD,    // vcc extension: _Yield [expr]
    EXPR_AWAIT,    // vcc extension: _Await expr
    EXPR_CO_OP     // vcc extension: __co_init(...) ... __co_alignof(...)
} ExprKind;

// The coroutine operations, in the order of their tokens (TOKEN_CO_INIT ...).
typedef enum {
    CO_OP_INIT,    // __co_init(storage, bytes, f, args...)
    CO_OP_ALLOCA,  // __co_alloca(f, extra, args...)
    CO_OP_RESUME,  // __co_resume(p)
    CO_OP_CANCEL,  // __co_cancel(p)
    CO_OP_DESTROY, // __co_destroy(p)
    CO_OP_DONE,    // __co_done(p)
    CO_OP_VALUE,   // __co_value(p)
    CO_OP_RESULT,  // __co_result(p)
    CO_OP_SIZEOF,  // __co_sizeof(f)
    CO_OP_ALIGNOF  // __co_alignof(f)
} CoOp;

typedef enum {
    UNARY_ADDRESS,
    UNARY_DEREF,
    UNARY_PLUS,
    UNARY_NEG,
    UNARY_BIT_NOT,
    UNARY_LOG_NOT,
    UNARY_PRE_INC,
    UNARY_PRE_DEC
} UnaryOp;

typedef enum {
    BINARY_MUL,
    BINARY_DIV,
    BINARY_MOD,
    BINARY_ADD,
    BINARY_SUB,
    BINARY_LEFT_SHIFT,
    BINARY_RIGHT_SHIFT,
    BINARY_LT,
    BINARY_GT,
    BINARY_LE,
    BINARY_GE,
    BINARY_EQ,
    BINARY_NE,
    BINARY_BIT_AND,
    BINARY_BIT_XOR,
    BINARY_BIT_OR,
    BINARY_LOG_AND,
    BINARY_LOG_OR,
    // Keep BINARY_COMMA last: the binary AST stream serializes the raw ordinal.
    BINARY_COMMA
} BinaryOp;

typedef enum {
    ASSIGN_SIMPLE,
    ASSIGN_MUL,
    ASSIGN_DIV,
    ASSIGN_MOD,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_LEFT,
    ASSIGN_RIGHT,
    ASSIGN_AND,
    ASSIGN_XOR,
    ASSIGN_OR
} AssignOp;

struct Expr {
    Expr *next; /* linked list for argument lists */
    SrcLoc loc; /* source position, for diagnostics */
    ExprKind kind;
    union {
        Literal *literal;
        Ident var;
        struct {
            UnaryOp op;
            Expr *expr;
        } unary_op;
        struct {
            BinaryOp op;
            Expr *left;
            Expr *right;
        } binary_op;
        struct {
            Expr *left;
            Expr *right;
        } subscript;
        struct {
            AssignOp op;
            Expr *target;
            Expr *value;
        } assign;
        struct {
            Expr *condition;
            Expr *then_expr;
            Expr *else_expr;
        } cond;
        struct {
            Type *type;
            Expr *expr;
        } cast;
        struct {
            Expr *func;
            Expr *args;
        } call;
        struct {
            Type *type;
            InitItem *init;
        } compound_literal;
        struct {
            Expr *expr;
            Ident field;
            int offset;        // byte offset within struct, set by typecheck
            Type *member_type; // declared type of the member, set by typecheck (see offset)
            BitField bf;       // a bit-field's storage unit (at offset), set by typecheck
        } field_access;
        struct {
            Expr *expr;
            Ident field;
            int offset;        // byte offset within struct, set by typecheck
            Type *member_type; // declared type of the member, set by typecheck (see offset)
            BitField bf;       // see field_access: the two must keep the same layout
        } ptr_access;
        Expr *post_inc;
        Expr *post_dec;
        Expr *sizeof_expr;
        Type *sizeof_type;
        Type *align_of;
        Type *va_class;
        Expr *yield_expr; /* optional */
        Expr *await_expr;
        struct {
            CoOp op;
            Expr *args; /* a coroutine is named by an EXPR_VAR */
        } co_op;
        struct {
            Expr *controlling_expr;
            GenericAssoc *associations;
        } generic;
    } u;
    Type *type; // for typecheck
};

typedef enum {
    LITERAL_INT,
    LITERAL_LONG,
    LITERAL_LONG_LONG,
    LITERAL_UINT,
    LITERAL_ULONG,
    LITERAL_ULONG_LONG,
    LITERAL_FLOAT,
    LITERAL_DOUBLE,
    LITERAL_LONG_DOUBLE,
    LITERAL_CHAR,
    LITERAL_STRING,
    LITERAL_ENUM
} LiteralKind;

//
// How an integer constant was spelled, which with the target's widths decides its type
// (C11 §6.4.4.1).  `parse` has no target, so it types the constant for the host; the
// semantic pass retypes it when LITERAL_SPELLED is set.  0 for a multi-byte character
// constant and for a literal the compiler makes itself.  An unsuffixed floating constant from the
// source has LITERAL_SPELLED alone, which says its single_val is set.
//
// LITERAL_CHAR_BYTE marks an unprefixed character constant of one byte, whose value is
// that of a plain char converted to int (C11 §6.4.4.4p10): negative for a byte over 127
// where plain char is signed.  `parse` keeps the byte; the semantic pass extends it.
//
enum {
    LITERAL_SPELLED   = 1,  // the bits below are valid
    LITERAL_DECIMAL   = 2,  // decimal: an unsigned type only with a U suffix
    LITERAL_SUFFIX_U  = 4,  // U
    LITERAL_SUFFIX_L  = 8,  // L
    LITERAL_SUFFIX_LL = 16, // LL
    LITERAL_CHAR_BYTE = 32, // a one-byte character constant, without LITERAL_SPELLED
};

struct Literal {
    LiteralKind kind;
    unsigned spelling; // LITERAL_SPELLED etc. for an integer constant, else 0
    float single_val;  // a double constant rounded once to binary32 (strtof), for a
                       // target whose double is single: valid when spelling is set
    union {
        int64_t int_val;
        long long_val;
        long long long_long_val;
        uint64_t uint_val;
        unsigned long ulong_val;
        unsigned long long ulong_long_val;
        double real_val;
        Float128 long_double_val; // binary128 bits, exact on any host
        char char_val;
        char *string_val;
        Ident enum_const;
    } u;
};

typedef enum { GENERIC_ASSOC_TYPE, GENERIC_ASSOC_DEFAULT } GenericAssocKind;

struct GenericAssoc {
    GenericAssoc *next; /* linked list */
    GenericAssocKind kind;
    union {
        struct {
            Type *type;
            Expr *expr;
        } type_assoc;
        Expr *default_assoc;
    } u;
};

/* Statements */
typedef enum {
    STMT_EXPR,
    STMT_COMPOUND,
    STMT_IF,
    STMT_SWITCH,
    STMT_WHILE,
    STMT_DO_WHILE,
    STMT_FOR,
    STMT_GOTO,
    STMT_CONTINUE,
    STMT_BREAK,
    STMT_RETURN,
    STMT_LABELED,
    STMT_CASE,
    STMT_DEFAULT,
    STMT_DEFER /* vcc extension: _Defer stmt */
} StmtKind;

struct Stmt {
    StmtKind kind;
    SrcLoc loc; /* source position, for diagnostics */
    union {
        Expr *expr; /* optional */
        DeclOrStmt *compound;
        struct {
            Expr *condition;
            Stmt *then_stmt;
            Stmt *else_stmt;
        } if_stmt;
        struct {
            Expr *expr;
            Stmt *body;
        } switch_stmt;
        struct {
            Expr *condition;
            Stmt *body;
        } while_stmt;
        struct {
            Stmt *body;
            Expr *condition;
        } do_while;
        struct {
            ForInit *init;
            Expr *condition;
            Expr *update;
            Stmt *body;
        } for_stmt;
        Ident goto_label;
        struct {
            Ident label;
            Stmt *stmt;
        } labeled;
        struct {
            Expr *expr;
            Stmt *stmt;
        } case_stmt;
        Stmt *default_stmt;
        Stmt *defer_stmt;
    } u;
    /* Assigned by label_loops() in translator; not serialized to .ast files. */
    char *loop_end_label;
    char *loop_continue_label;
    char *branch_target_label;
};

typedef enum { DECL_OR_STMT_DECL, DECL_OR_STMT_STMT } DeclOrStmtKind;

struct DeclOrStmt {
    DeclOrStmt *next; /* linked list */
    DeclOrStmtKind kind;
    union {
        Declaration *decl;
        Stmt *stmt;
    } u;
};

typedef enum { FOR_INIT_EXPR, FOR_INIT_DECL } ForInitKind;

struct ForInit {
    ForInitKind kind;
    union {
        Expr *expr;
        Declaration *decl;
    } u;
};

/* Program Structure */
struct Program {
    ExternalDecl *decls;
};

typedef enum { EXTERNAL_DECL_FUNCTION, EXTERNAL_DECL_DECLARATION } ExternalDeclKind;

struct ExternalDecl {
    ExternalDecl *next; /* linked list */
    SrcLoc loc;         /* source position, for diagnostics */
    ExternalDeclKind kind;
    union {
        struct {
            Type *type;
            Ident name;
            DeclSpec *specifiers;
            Declaration *param_decls;
            Stmt *body;
        } function;
        Declaration *declaration;
    } u;
};

//
// Allocate
//
Type *new_type(TypeKind kind, const char *funcname, const char *filename, unsigned lineno);
TypeQualifier *new_type_qualifier(TypeQualifierKind kind);
Field *new_field(FieldKind kind);
Enumerator *new_enumerator(Ident name, Expr *value);
Param *new_param(void);
Declaration *new_declaration(DeclarationKind kind);
DeclSpec *new_decl_spec(void);
FunctionSpec *new_function_spec(FunctionSpecKind kind);
AlignmentSpec *new_alignment_spec(AlignmentSpecKind kind);
InitDeclarator *new_init_declarator(void);
Initializer *new_initializer(InitializerKind kind);
InitItem *new_init_item(Designator *designators, Initializer *init);
Designator *new_designator(DesignatorKind kind);
Expr *new_expression(ExprKind kind);
Literal *new_literal(LiteralKind kind);
GenericAssoc *new_generic_assoc(GenericAssocKind kind);
Stmt *new_stmt(StmtKind kind);
DeclOrStmt *new_decl_or_stmt(DeclOrStmtKind kind);
ForInit *new_for_init(ForInitKind kind);
ExternalDecl *new_external_decl(ExternalDeclKind kind);
Program *new_program(void);

//
// Deallocate
//
void free_type(Type *type);
void free_type_qualifier(TypeQualifier *qual);
void free_field(Field *field);
void free_enumerator(Enumerator *enumerator);
void free_param(Param *param);
void free_declaration(Declaration *decl);
void free_decl_spec(DeclSpec *spec);
void free_function_spec(FunctionSpec *fs);
void free_alignment_spec(AlignmentSpec *as);
void free_init_declarator(InitDeclarator *init_decl);
void free_initializer(Initializer *init);
void free_init_item(InitItem *item);
void free_designator(Designator *design);
void free_expression(Expr *expr);
void free_literal(Literal *lit);
void free_generic_assoc(GenericAssoc *assoc);
void free_statement(Stmt *stmt);
void free_decl_or_stmt(DeclOrStmt *ds);
void free_for_init(ForInit *fi);
void free_external_decl(ExternalDecl *ext_decl);
void free_program(Program *program);

//
// Clone
//
Type *clone_type(const Type *type, const char *funcname, const char *filename, unsigned lineno);
TypeQualifier *clone_type_qualifier(const TypeQualifier *qualifier);
Param *clone_param(const Param *param);
Expr *clone_expression(const Expr *expression);
AlignmentSpec *clone_alignment_spec(const AlignmentSpec *as);
Literal *clone_literal(const Literal *literal);
GenericAssoc *clone_generic_assoc(const GenericAssoc *assoc);
InitItem *clone_init_item(const InitItem *item);
Field *clone_field(const Field *field);
Enumerator *clone_enumerator(const Enumerator *enumerator);

//
// Compare
//
bool compare_type(const Type *a, const Type *b);
bool compare_type_qualifier(const TypeQualifier *a, const TypeQualifier *b);
bool compare_field(const Field *a, const Field *b);
bool compare_enumerator(const Enumerator *a, const Enumerator *b);
bool compare_param(const Param *a, const Param *b);
bool compare_declaration(const Declaration *a, const Declaration *b);
bool compare_decl_spec(const DeclSpec *a, const DeclSpec *b);
bool compare_function_spec(const FunctionSpec *a, const FunctionSpec *b);
bool compare_alignment_spec(const AlignmentSpec *a, const AlignmentSpec *b);
bool compare_init_declarator(const InitDeclarator *a, const InitDeclarator *b);
bool compare_initializer(const Initializer *a, const Initializer *b);
bool compare_init_item(const InitItem *a, const InitItem *b);
bool compare_designator(const Designator *a, const Designator *b);
bool compare_expr(const Expr *a, const Expr *b);
bool compare_literal(const Literal *a, const Literal *b);
bool compare_generic_assoc(const GenericAssoc *a, const GenericAssoc *b);
bool compare_stmt(const Stmt *a, const Stmt *b);
bool compare_decl_or_stmt(const DeclOrStmt *a, const DeclOrStmt *b);
bool compare_for_init(const ForInit *a, const ForInit *b);
bool compare_external_decl(const ExternalDecl *a, const ExternalDecl *b);
bool compare_program(const Program *a, const Program *b);

//
// Import, export
//
Program *import_ast(int fildes);
void export_ast(int fildes, Program *program);
void export_yaml(FILE *fd, Program *program);
void export_dot(FILE *fd, Program *program);

typedef struct _wfile WFILE;
void ast_import_open(WFILE *input, int fildes);
ExternalDecl *import_external_decl(WFILE *input);

//
// Print
//
void print_program(FILE *fd, Program *program);
void print_expression(FILE *fd, const Expr *expr, int indent);
void print_statement(FILE *fd, Stmt *stmt, int indent);
void print_type(FILE *fd, const Type *type, int indent);
void print_type_qualifiers(FILE *fd, const TypeQualifier *qualifiers, int indent);
void print_declaration(FILE *fd, Declaration *decl, int indent);
void print_external_decl(FILE *fd, ExternalDecl *ext, int indent);
void print_initializer(FILE *fd, const Initializer *init, int indent);
extern const char *type_kind_str[];

// The type as C writes it, for a diagnostic: "const char *", "int (*)(int)",
// "struct <anonymous>". The string is xalloc'ed.
char *type_to_c(const Type *type);
extern const char *co_op_name[]; // "__co_init" ... "__co_alignof", by CoOp

#ifdef __cplusplus
}
#endif

#endif /* AST_H */
