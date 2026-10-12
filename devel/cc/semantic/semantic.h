//
// Internal types for translator.
//
#ifndef SEMANTIC_H
#define SEMANTIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "tac.h"

// Level of scope for nested compound operators.
extern int scope_level;

// Enable debug output
extern int semantic_debug;
extern int xalloc_debug;

// Semantic analysis entry points: type-check and label loops.  `seq` is the
// caller-owned, unit-wide label/temp counter (see translate.h): typecheck_decl
// forwards it to label_loops, which shares it with the translator's temporaries.
void typecheck_decl(ExternalDecl *d, int *seq);
void typecheck_program(const Program *p);

// Annotate loops and break/continue statements, drawing label numbers from *seq.
void label_loops(const ExternalDecl *ast, int *seq);

// Label the loops of one statement anew (a deferred statement the translator lowers
// once more), drawing label numbers from *seq.
void label_loops_stmt(Stmt *stmt, int *seq);

// Validate labeled statements and goto targets within a function.
void resolve_labels(const ExternalDecl *ast);

// Check what deferred statements may not do: be left by return, break, continue or
// goto, or be jumped past by a goto or a case label (docs/Coroutines_in_C.md, 1).
void check_defers(const ExternalDecl *ast);

// After an error unwound out of typecheck_decl(d), forget the declaration's block
// scopes and per-function state, so the next declaration starts afresh; a name d
// failed to declare is no new error where it is used.
void semantic_recover(const ExternalDecl *d);

// Error handling.
#ifdef __cplusplus
[[noreturn]]
#else
_Noreturn
#endif
void fatal_error(const char *message, ...);

// Convert literal to given arithmetic type and return as Tac_StaticInit.
Tac_StaticInit *new_static_init_from_literal(const Type *type, const Literal *lit);

// Static initializer slot for an integer (or integer-valued pointer) object of
// `size` bytes: I16/U16 for a 2-byte object, I32/U32 for a 4-byte one, I64/U64 for
// anything wider.
Tac_StaticInit *new_static_init_int(size_t size, bool is_signed, uint64_t bits);

// Reject a character constant (an int/unsigned literal with no spelling) that does not
// fit the target's int/unsigned int.  Only a multi-character constant can be that wide.
void check_int_literal_width(const Literal *lit);

// A one-byte character constant (LITERAL_CHAR_BYTE) takes the value of a plain char
// converted to int: its byte sign-extended where plain char is signed.  Idempotent.
void type_char_literal(Literal *lit);

// Give an integer constant its type by its spelling and the target's widths (C11
// §6.4.4.1): `parse` typed it for the host.  A literal with no spelling is left alone.
void type_int_literal(Literal *lit);

// Convert any arithmetic literal to a real value.
double literal_to_double(const Literal *lit);
// The exact binary128 value of a numeric literal.
Float128 literal_to_long_double(const Literal *lit);

//
// Helpers for Type.
//
size_t get_size(const Type *t);
size_t get_alignment(const Type *t);
// The alignment a declaration's _Alignas asks, once type-checked: 0 for none.
int alignas_bytes(const DeclSpec *spec);
bool is_complete(const Type *t);
bool is_scalar(const Type *t);
bool is_arithmetic(const Type *t);
bool is_integer(const Type *t);
bool is_character(const Type *t);
bool is_promotable_narrow(const Type *t); // integer types the promotions widen to int
bool ushort_promotes_unsigned(void);      // unsigned short promotes to unsigned int
TypeKind promoted_kind(const Type *t);    // TYPE_INT or TYPE_UINT, for a narrow type
TypeKind ptrdiff_kind(void);              // the target's ptrdiff_t: as wide as a pointer
TypeKind size_kind(void);                 // the target's size_t: unsigned ptrdiff_t
bool is_pointer(const Type *t);
bool is_array(const Type *t);
bool is_complete_pointer(const Type *t);
bool is_signed(const Type *t);
// The value width in bits of integer type `t`: a signed type's value bits (a BESM-6 int
// has 41 inside its 48-bit word), all the storage bits of an unsigned one, 1 for _Bool.
int integer_value_bits(const Type *t);
// The unsigned integer type of `size` bytes (TYPE_UCHAR ... TYPE_ULONG_LONG; unsigned
// int rather than an equal unsigned short or long), or TYPE_VOID when there is none.
TypeKind unsigned_kind_of_size(int size);
// The signed integer type of the same size as unsigned kind `k`.
TypeKind signed_kind_of(TypeKind k);
// The bit-field record of a member access, or NULL when it is not a bit-field.
const BitField *access_bitfield(const Expr *e);
bool type_is_volatile(const Type *t);
int round_away_from_zero(int alignment, int size);
Type *resolve_typedef_names(Type *t);
const Type *unalias(const Type *t);
// The TAC form of type `t`, struct members included (tac_type.c).
Tac_Type *ast_type_to_tac_type(const Type *t);
// __builtin_va_class(t) on the active target (Target.va_class).
int va_class_of(const Type *t);

#ifdef __cplusplus
}
#endif

#endif /* SEMANTIC_H */
