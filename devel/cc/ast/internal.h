//
// Internal types for parser, hidden from consumers of AST.
//
#ifndef AST_INTERNAL_H
#define AST_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "ast.h"

/* Forward declarations for recursive types */
typedef struct Declarator Declarator;
typedef struct DeclaratorSuffix DeclaratorSuffix;
typedef struct Pointer Pointer;
typedef struct TypeSpec TypeSpec;

typedef enum {
    TYPE_SPEC_BASIC,
    TYPE_SPEC_STRUCT,
    TYPE_SPEC_UNION,
    TYPE_SPEC_ENUM,
    TYPE_SPEC_TYPEDEF_NAME,
    TYPE_SPEC_ATOMIC,
    TYPE_SPEC_CORO_FRAME // _Coro_frame(Y, T): the finished frame type in u.atomic.type
} TypeSpecKind;          // Internal for parser only

struct TypeSpec {   // Internal for parser only
    TypeSpec *next; /* linked list */
    TypeSpecKind kind;
    union {
        Type *basic;
        struct {
            Ident name;
            Field *fields;
        } struct_spec; /* optional name */
        struct {
            Ident name;
            Enumerator *enumerators;
        } enum_spec;
        struct {
            Ident name;
        } typedef_name;
        struct {
            Type *type;
        } atomic;
    } u;
    TypeQualifier *qualifiers; /* attributes */
};

struct Declarator {
    Declarator *next; /* linked list */
    Ident name;       /* NULL for abstract declarator */
    SrcLoc loc;       /* of the name */
    Pointer *pointers;
    DeclaratorSuffix *suffixes;
};

struct Pointer {
    Pointer *next; /* linked list */
    TypeQualifier *qualifiers;
};

typedef enum { SUFFIX_ARRAY, SUFFIX_FUNCTION, SUFFIX_POINTER } DeclaratorSuffixKind;

struct DeclaratorSuffix {
    DeclaratorSuffix *next; /* linked list */
    DeclaratorSuffixKind kind;
    union {
        struct {
            Expr *size;
            TypeQualifier *qualifiers;
            bool is_static;
        } array;
        struct {
            Param *params;
            bool variadic;
        } function;
        struct {
            Pointer *pointers;
            DeclaratorSuffix *suffix;
        } pointer;
    } u;
};

//
// Print
//
void print_declarator(FILE *fd, Declarator *decl, int indent);
void print_type_spec(FILE *fd, const TypeSpec *spec, int indent);

//
// Allocate
//
Declarator *new_declarator(void);
DeclaratorSuffix *new_declarator_suffix(DeclaratorSuffixKind kind);
Pointer *new_pointer(void);
TypeSpec *new_type_spec(TypeSpecKind kind);

//
// Deallocate
//
void free_declarator(Declarator *decl);
void free_declarator_suffix(DeclaratorSuffix *suffix);
void free_pointer(Pointer *ptr);
void free_type_spec(TypeSpec *ts);

//
// Clone
//
TypeSpec *clone_type_spec(const TypeSpec *ts);

#ifdef __cplusplus
}
#endif

#endif /* AST_INTERNAL_H */
