#include <stdio.h>
#include <string.h>

#include "parser_internal.h"
#include "xalloc.h"

static char *strip_string_literal_lexeme(const char *lex)
{
    size_t n = strlen(lex);
    if (n >= 2 && lex[0] == '"' && lex[n - 1] == '"') {
        char *s = xalloc(n - 1, __func__, __FILE__, __LINE__);
        memcpy(s, lex + 1, n - 2);
        s[n - 2] = '\0';
        return s;
    }
    return xstrdup(lex);
}

//
// Synthetic tags for anonymous struct/union definitions.
//
// A tagless definition is ONE type however many declarators share it: `struct { int x; } a, b;`
// declares two objects of the same type, and so does `struct W { struct { int x; } p, q; };`.
// The tag is therefore minted here, at the single point where the Type node for a definition is
// built, and *before* parse_init_declarator / parse_struct_declaration clone the base type per
// declarator -- clone_type copies the tag, so every declarator names the same struct.  Minting it
// later, per cloned node (as the type registrar used to), split one type into N mutually
// incompatible ones.  `__anon_` is in the implementation's reserved namespace, so it cannot
// collide with a user tag.  The counter is per translation unit; parse() resets it.
//
static int anon_tag_counter;

void reset_anon_tag_counter(void)
{
    anon_tag_counter = 0;
}

//
// Set by parse_declaration_specifiers for a typedef declaration whose type is already
// specified, as in the second of `typedef struct S T; typedef struct S T;`.  A typedef name
// there is the declarator, not a type specifier: C11 §6.7p3 allows a typedef to be repeated
// with the same type (the semantic pass checks that it is the same).
//
static bool typedef_redeclaration;

//
// The tag of a struct/union specifier: the source tag when it has one, a fresh synthetic tag when
// it is an anonymous *definition*.  A specifier with neither (`struct;`) keeps a NULL name, which
// is what xstrdup(NULL) yielded before.
//
static char *struct_tag_name(const TypeSpec *spec)
{
    if (spec->u.struct_spec.name)
        return xstrdup(spec->u.struct_spec.name);
    if (!spec->u.struct_spec.fields)
        return NULL;
    char buf[32];
    snprintf(buf, sizeof(buf), "__anon_%d", ++anon_tag_counter);
    return xstrdup(buf);
}

//
// Fuse TypeSpec list into a single Type.
// Returns non-NULL value.
//
Type *fuse_type_specifiers(const TypeSpec *specs)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!specs) {
        // An identifier where a type belongs: a misspelled or undeclared type name if a
        // declarator follows it, else a declaration with no type at all.
        if (current_token == TOKEN_IDENTIFIER &&
            (next_token() == TOKEN_IDENTIFIER || next_token() == TOKEN_STAR))
            fatal_error("unknown type name '%s'", current_lexeme);
        fatal_error("expected a type %s", parser_where());
    }

    enum { SIGNED_SIGNED, SIGNED_UNSIGNED };

    /* State for tracking type specifiers */
    int base_kind                = -1; /* Unset; holds TypeKind values */
    int signedness               = -1; /* Unset */
    int int_count                = 0;  /* For int */
    int long_count               = 0;  /* For long, long long */
    bool is_complex              = false;
    bool is_imaginary            = false;
    const TypeSpec *struct_spec  = NULL;
    const TypeSpec *union_spec   = NULL;
    const TypeSpec *enum_spec    = NULL;
    const TypeSpec *typedef_spec = NULL;
    const TypeSpec *atomic_spec  = NULL; // _Atomic(type) or _Coro_frame(Y, T)

    /* Collect specifiers */
    for (const TypeSpec *s = specs; s; s = s->next) {
        if (s->kind == TYPE_SPEC_BASIC) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec) {
                fatal_error(
                    "cannot combine a type specifier with a struct, union, enum or typedef name");
            }
            switch (s->u.basic->kind) {
            case TYPE_VOID:
                if (base_kind != -1) {
                    fatal_error("cannot combine 'void' with another type");
                }
                base_kind = TYPE_VOID;
                break;
            case TYPE_BOOL:
                if (base_kind != -1) {
                    fatal_error("cannot combine '_Bool' with another type");
                }
                base_kind = TYPE_BOOL;
                break;
            case TYPE_CHAR:
                if (base_kind != -1) {
                    fatal_error("cannot combine 'char' with '%s'", type_kind_str[base_kind]);
                }
                base_kind = TYPE_CHAR;
                break;
            case TYPE_SHORT:
                if (base_kind != -1 && base_kind != TYPE_INT) {
                    fatal_error("cannot combine 'short' with '%s'", type_kind_str[base_kind]);
                }
                base_kind = TYPE_SHORT;
                break;
            case TYPE_INT:
                if (base_kind != -1 && base_kind != TYPE_SHORT && base_kind != TYPE_LONG) {
                    fatal_error("cannot combine 'int' with '%s'", type_kind_str[base_kind]);
                }
                if (int_count > 0) {
                    fatal_error("duplicate 'int'");
                }
                int_count++;
                if (base_kind == -1) {
                    base_kind = TYPE_INT;
                }
                break;
            case TYPE_LONG:
                if (base_kind != -1 && base_kind != TYPE_INT && base_kind != TYPE_LONG &&
                    base_kind != TYPE_DOUBLE) {
                    fatal_error("cannot combine 'long' with '%s'", type_kind_str[base_kind]);
                }
                if (long_count > 2) {
                    fatal_error("'long long long' is too long");
                }
                long_count++;
                if (base_kind == TYPE_DOUBLE) {
                    base_kind = TYPE_LONG_DOUBLE;
                } else if (long_count == 2) {
                    base_kind = TYPE_LONG_LONG;
                } else {
                    base_kind = TYPE_LONG;
                }
                break;
            case TYPE_FLOAT:
                if (base_kind != -1 && base_kind != TYPE_COMPLEX && base_kind != TYPE_IMAGINARY) {
                    fatal_error("cannot combine 'float' with '%s'", type_kind_str[base_kind]);
                }
                base_kind = TYPE_FLOAT;
                break;
            case TYPE_DOUBLE:
                if (base_kind != -1 && base_kind != TYPE_LONG && base_kind != TYPE_COMPLEX &&
                    base_kind != TYPE_IMAGINARY) {
                    fatal_error("cannot combine 'double' with '%s'", type_kind_str[base_kind]);
                }
                base_kind = (base_kind == TYPE_LONG) ? TYPE_LONG_DOUBLE : TYPE_DOUBLE;
                break;
            case TYPE_SIGNED:
                if (base_kind != -1 && base_kind != TYPE_CHAR && base_kind != TYPE_SHORT &&
                    base_kind != TYPE_INT && base_kind != TYPE_LONG) {
                    fatal_error("cannot combine 'signed' with '%s'", type_kind_str[base_kind]);
                }
                if (signedness == SIGNED_SIGNED)
                    fatal_error("duplicate 'signed'");
                if (signedness == SIGNED_UNSIGNED)
                    fatal_error("cannot combine 'signed' with 'unsigned'");
                signedness = SIGNED_SIGNED;
                break;
            case TYPE_UNSIGNED:
                if (base_kind != -1 && base_kind != TYPE_CHAR && base_kind != TYPE_SHORT &&
                    base_kind != TYPE_INT && base_kind != TYPE_LONG) {
                    fatal_error("cannot combine 'unsigned' with '%s'", type_kind_str[base_kind]);
                }
                if (signedness == SIGNED_UNSIGNED)
                    fatal_error("duplicate 'unsigned'");
                if (signedness == SIGNED_SIGNED)
                    fatal_error("cannot combine 'unsigned' with 'signed'");
                signedness = SIGNED_UNSIGNED;
                break;
            case TYPE_COMPLEX:
                if (base_kind != -1 && base_kind != TYPE_FLOAT && base_kind != TYPE_DOUBLE) {
                    fatal_error("cannot combine '_Complex' with '%s'", type_kind_str[base_kind]);
                }
                is_complex = true;
                if (base_kind == -1)
                    base_kind = TYPE_DOUBLE; /* Default for _Complex */
                break;
            case TYPE_IMAGINARY:
                if (base_kind != -1 && base_kind != TYPE_FLOAT && base_kind != TYPE_DOUBLE) {
                    fatal_error("cannot combine '_Imaginary' with '%s'", type_kind_str[base_kind]);
                }
                is_imaginary = true;
                if (base_kind == -1)
                    base_kind = TYPE_DOUBLE; /* Default for _Imaginary */
                break;
            default:
                internal_error("unknown basic type specifier");
            }
        } else if (s->kind == TYPE_SPEC_STRUCT) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec ||
                base_kind != -1) {
                fatal_error("cannot combine 'struct' with another type");
            }
            struct_spec = s;
        } else if (s->kind == TYPE_SPEC_UNION) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec ||
                base_kind != -1) {
                fatal_error("cannot combine 'union' with another type");
            }
            union_spec = s;
        } else if (s->kind == TYPE_SPEC_ENUM) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec ||
                base_kind != -1) {
                fatal_error("cannot combine 'enum' with another type");
            }
            enum_spec = s;
        } else if (s->kind == TYPE_SPEC_TYPEDEF_NAME) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec ||
                base_kind != -1) {
                fatal_error("cannot combine a typedef name with another type");
            }
            typedef_spec = s;
        } else if (s->kind == TYPE_SPEC_ATOMIC || s->kind == TYPE_SPEC_CORO_FRAME) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec ||
                base_kind != -1) {
                fatal_error("cannot combine '%s' with another type",
                            s->kind == TYPE_SPEC_ATOMIC ? "_Atomic(type)" : "_Coro_frame");
            }
            atomic_spec = s;
        }
    }

    /* Validate and construct Type */
    Type *result = NULL;

    if (struct_spec) {
        result                    = new_type(TYPE_STRUCT, __func__, __FILE__, __LINE__);
        result->u.struct_t.name   = struct_tag_name(struct_spec);
        result->u.struct_t.fields = clone_field(struct_spec->u.struct_spec.fields);
    } else if (union_spec) {
        result                    = new_type(TYPE_UNION, __func__, __FILE__, __LINE__);
        result->u.struct_t.name   = struct_tag_name(union_spec);
        result->u.struct_t.fields = clone_field(union_spec->u.struct_spec.fields);
    } else if (enum_spec) {
        result                       = new_type(TYPE_ENUM, __func__, __FILE__, __LINE__);
        result->u.enum_t.name        = xstrdup(enum_spec->u.enum_spec.name);
        result->u.enum_t.enumerators = clone_enumerator(enum_spec->u.enum_spec.enumerators);
    } else if (typedef_spec) {
        result                      = new_type(TYPE_TYPEDEF_NAME, __func__, __FILE__, __LINE__);
        result->u.typedef_name.name = xstrdup(typedef_spec->u.typedef_name.name);
    } else if (atomic_spec && atomic_spec->kind == TYPE_SPEC_CORO_FRAME) {
        result = clone_type(atomic_spec->u.atomic.type, __func__, __FILE__, __LINE__);
    } else if (atomic_spec) {
        result = new_type(TYPE_ATOMIC, __func__, __FILE__, __LINE__);
        result->u.atomic.base =
            clone_type(atomic_spec->u.atomic.type, __func__, __FILE__, __LINE__);
    } else {
        /* Handle basic types */
        if (base_kind == -1) {
            if (signedness == -1) {
                internal_error("no valid type specifier provided");
            }
            base_kind = (signedness == SIGNED_SIGNED) ? TYPE_INT : TYPE_UINT;
        }
        if (is_complex && is_imaginary) {
            fatal_error("cannot combine '_Complex' with '_Imaginary'");
        }
        if ((is_complex || is_imaginary) && (base_kind != TYPE_FLOAT && base_kind != TYPE_DOUBLE)) {
            fatal_error("'_Complex' and '_Imaginary' require 'float' or 'double'");
        }
        if ((signedness == SIGNED_SIGNED || signedness == SIGNED_UNSIGNED || long_count > 0) &&
            (base_kind == TYPE_FLOAT || base_kind == TYPE_DOUBLE)) {
            fatal_error("cannot combine 'signed', 'unsigned' or 'long' with 'float' or 'double'");
        }
        if (base_kind == TYPE_VOID || base_kind == TYPE_BOOL) {
            if (long_count > 0 || signedness == SIGNED_UNSIGNED || is_complex || is_imaginary) {
                fatal_error("cannot combine 'void' or '_Bool' with another type specifier");
            }
        }

        // Update base type for signedness.
        switch (signedness) {
        case SIGNED_SIGNED:
            if (base_kind == TYPE_CHAR)
                base_kind = TYPE_SCHAR;
            break;
        case SIGNED_UNSIGNED:
            switch (base_kind) {
            case TYPE_CHAR:
                base_kind = TYPE_UCHAR;
                break;
            case TYPE_SHORT:
                base_kind = TYPE_USHORT;
                break;
            case TYPE_INT:
                base_kind = TYPE_UINT;
                break;
            case TYPE_LONG:
                base_kind = TYPE_ULONG;
                break;
            case TYPE_LONG_LONG:
                base_kind = TYPE_ULONG_LONG;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

        /* Create Type based on base_kind */
        if (is_complex) {
            result                 = new_type(TYPE_COMPLEX, __func__, __FILE__, __LINE__);
            result->u.complex.base = new_type((TypeKind)base_kind, __func__, __FILE__, __LINE__);
        } else if (is_imaginary) {
            result                 = new_type(TYPE_IMAGINARY, __func__, __FILE__, __LINE__);
            result->u.complex.base = new_type((TypeKind)base_kind, __func__, __FILE__, __LINE__);
        } else {
            result = new_type((TypeKind)base_kind, __func__, __FILE__, __LINE__);
        }
    }
    return result;
}

Type *type_apply_pointers(Type *type, const Pointer *pointers)
{
    for (const Pointer *p = pointers; p; p = p->next) {
        Type *ptr                 = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
        ptr->u.pointer.target     = type;
        ptr->u.pointer.qualifiers = clone_type_qualifier(p->qualifiers);
        ptr->qualifiers           = NULL;
        type                      = ptr;
    }
    return type;
}

// Reject a malformed constant array dimension at parse time: a non-integer
// (e.g. int x[2.0]) or a negative (e.g. int arr[-3]) size.  A non-constant size
// (a variable, VLA) and an unsized [] (size == NULL) pass through untouched.
static void validate_array_size(const Expr *size)
{
    if (!size)
        return;
    if (size->kind == EXPR_LITERAL) {
        switch (size->u.literal->kind) {
        case LITERAL_FLOAT:
        case LITERAL_DOUBLE:
        case LITERAL_LONG_DOUBLE:
            fatal_error("array size must have integer type");
        default:
            break;
        }
        return;
    }
    // A negated integer constant, e.g. int arr[-3] -> UNARY_NEG of LITERAL_INT.
    if (size->kind == EXPR_UNARY_OP && size->u.unary_op.op == UNARY_NEG) {
        const Expr *operand = size->u.unary_op.expr;
        if (operand && operand->kind == EXPR_LITERAL) {
            switch (operand->u.literal->kind) {
            case LITERAL_FLOAT:
            case LITERAL_DOUBLE:
            case LITERAL_LONG_DOUBLE:
                fatal_error("array size must have integer type");
            default:
                fatal_error("array size must be positive");
            }
        }
    }
}

Type *type_apply_suffixes(Type *type, const DeclaratorSuffix *suffixes)
{
    if (!suffixes)
        return type;
    const DeclaratorSuffix *s = suffixes;
    switch (s->kind) {
    case SUFFIX_ARRAY: {
        // Recurse on remaining suffixes first so the leftmost bracket becomes
        // the outermost array dimension, matching C semantics for foo[3][1].
        type = type_apply_suffixes(type, s->next);
        validate_array_size(s->u.array.size);
        Type *array               = new_type(TYPE_ARRAY, __func__, __FILE__, __LINE__);
        array->u.array.element    = type;
        array->u.array.size       = clone_expression(s->u.array.size);
        array->u.array.qualifiers = clone_type_qualifier(s->u.array.qualifiers);
        array->u.array.is_static  = s->u.array.is_static;
        array->qualifiers         = NULL;
        return array;
    }
    case SUFFIX_FUNCTION: {
        type                         = type_apply_suffixes(type, s->next);
        Type *func                   = new_type(TYPE_FUNCTION, __func__, __FILE__, __LINE__);
        func->u.function.return_type = type;
        func->u.function.params      = clone_param(s->u.function.params);
        func->u.function.variadic    = s->u.function.variadic;
        func->qualifiers             = NULL;
        return func;
    }
    case SUFFIX_POINTER:
        type = type_apply_suffixes(type, s->next);
        type = type_apply_pointers(type, s->u.pointer.pointers);
        return type_apply_suffixes(type, s->u.pointer.suffix);
    }
    return type;
}

//
// Is this a typedef?
//
bool is_typedef(const DeclSpec *specifiers)
{
    if (!specifiers)
        return false;
    return specifiers->storage == STORAGE_CLASS_TYPEDEF;
}

//
// Define all names in this declarator as typedef.
//
void define_typedef(InitDeclarator *decl)
{
    for (; decl; decl = decl->next) {
        int token = nametab_find(decl->name);
        if (!token || token == TOKEN_TYPEDEF_NAME) {
            nametab_define(decl->name, TOKEN_TYPEDEF_NAME, scope_level);
        } else {
            fatal_error("redefinition of typedef '%s'", decl->name);
        }
    }
}

//
// declaration
//     : declaration_specifiers ';'
//     | declaration_specifiers init_declarator_list ';'
//     | static_assert_declaration
//     ;
//
Declaration *parse_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {
        return parse_static_assert_declaration();
    }
    SrcLoc loc           = diag_loc; // of its first token
    Type *base_type      = NULL;
    DeclSpec *specifiers = parse_declaration_specifiers(&base_type);
    if (current_token == TOKEN_SEMICOLON) {
        advance_token();
        Declaration *decl        = new_declaration(DECL_EMPTY);
        decl->loc                = loc;
        decl->u.empty.specifiers = specifiers;
        decl->u.empty.type       = base_type;
        return decl;
    }
    InitDeclarator *declarators = parse_init_declarator_list(NULL, base_type);
    expect_token(TOKEN_SEMICOLON);
    free_type(base_type);
    Declaration *decl       = new_declaration(DECL_VAR);
    decl->loc               = loc;
    decl->u.var.specifiers  = specifiers;
    decl->u.var.declarators = declarators;
    if (is_typedef(specifiers)) {
        define_typedef(declarators);
    }
    return decl;
}

//
// declaration_specifiers
//     : storage_class_specifier declaration_specifiers
//     | storage_class_specifier
//     | type_specifier declaration_specifiers
//     | type_specifier
//     | type_qualifier declaration_specifiers
//     | type_qualifier
//     | function_specifier declaration_specifiers
//     | function_specifier
//     | alignment_specifier declaration_specifiers
//     | alignment_specifier
//     ;
// Stores base type by provided pointer.
// Returns DeclSpec object when it's not empty.
// Otherwise returns NULL.
//
DeclSpec *parse_declaration_specifiers(Type **base_type_result)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    DeclSpec *ds         = new_decl_spec();
    TypeSpec *type_specs = NULL;
    while (1) {
        if (is_storage_class_specifier(current_token)) {
            // A declaration may include at most one storage-class specifier
            // (C11 §6.7.1p2): reject "static extern", "static int extern", etc.
            if (ds->storage != STORAGE_CLASS_NONE) {
                fatal_error("more than one storage class in a declaration");
            }
            ds->storage = parse_storage_class_specifier();
        } else if (current_token == TOKEN_TYPEDEF_NAME && type_specs &&
                   ds->storage == STORAGE_CLASS_TYPEDEF) {
            break; // a repeated typedef: the name is the declarator
        } else if (is_type_specifier(current_token) ||
                   (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN)) {
            TypeSpec *ts = parse_type_specifier();
            append_list(&type_specs, ts);
        } else if (is_type_qualifier(current_token) || current_token == TOKEN_ATOMIC) {
            TypeQualifier *q = parse_type_qualifier();
            append_list(&ds->qualifiers, q);
        } else if (current_token == TOKEN_INLINE || current_token == TOKEN_NORETURN ||
                   current_token == TOKEN_CORO) {
            FunctionSpec *fs = parse_function_specifier();
            append_list(&ds->func_specs, fs);
        } else if (current_token == TOKEN_ALIGNAS) {
            parse_one_alignment_specifier(&ds->align_spec);
        } else {
            break;
        }
    }
    typedef_redeclaration = ds->storage == STORAGE_CLASS_TYPEDEF && type_specs;
    *base_type_result     = fuse_type_specifiers(type_specs);
    free_type_spec(type_specs);
    // Qualifiers from the declaration specifiers (const/volatile/...) qualify the
    // base type being declared. Attach them to the base type so every declarator
    // built from it inherits them via clone_type (e.g. `volatile int *p` yields a
    // pointer to volatile int). Pointer-level qualifiers are handled separately by
    // type_apply_pointers. Mirrors parse_type_name's handling of the same case.
    if (*base_type_result && ds->qualifiers)
        (*base_type_result)->qualifiers = clone_type_qualifier(ds->qualifiers);
    if (!ds->storage && !ds->qualifiers && !ds->func_specs && !ds->align_spec) {
        free_decl_spec(ds);
        return NULL;
    }
    return ds;
}

//
// init_declarator_list
//     : init_declarator
//     | init_declarator_list ',' init_declarator
//     ;
//
InitDeclarator *parse_init_declarator_list(Declarator *first, const Type *base_type)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    InitDeclarator *decl = parse_init_declarator(first, base_type);
    if (current_token == TOKEN_COMMA) {
        advance_token();
        decl->next = parse_init_declarator_list(NULL, base_type);
    }
    return decl;
}

//
// init_declarator
//     : declarator '=' initializer
//     | declarator
//     ;
//
InitDeclarator *parse_init_declarator(Declarator *decl, const Type *base_type)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
        if (base_type)
            print_type(stdout, base_type, 4);
    }
    if (!decl) {
        decl = parse_declarator();
    }
    InitDeclarator *init_decl = new_init_declarator();
    if (decl->name) {
        init_decl->loc = decl->loc;
    }
    init_decl->name = decl->name;
    decl->name      = NULL;
    if (current_token == TOKEN_ASSIGN) {
        advance_token();
        init_decl->init = parse_initializer();
    }
    init_decl->type = type_apply_suffixes(
        type_apply_pointers(clone_type(base_type, __func__, __FILE__, __LINE__), decl->pointers),
        decl->suffixes);
    free_declarator(decl);
    return init_decl;
}

//
// storage_class_specifier
//     : TYPEDEF   /* identifiers must be flagged as TYPEDEF_NAME */
//     | EXTERN
//     | STATIC
//     | THREAD_LOCAL
//     | AUTO
//     | REGISTER
//     ;
//
StorageClass parse_storage_class_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    StorageClass kind = current_token == TOKEN_TYPEDEF        ? STORAGE_CLASS_TYPEDEF
                        : current_token == TOKEN_EXTERN       ? STORAGE_CLASS_EXTERN
                        : current_token == TOKEN_STATIC       ? STORAGE_CLASS_STATIC
                        : current_token == TOKEN_THREAD_LOCAL ? STORAGE_CLASS_THREAD_LOCAL
                        : current_token == TOKEN_AUTO         ? STORAGE_CLASS_AUTO
                        : current_token == TOKEN_REGISTER     ? STORAGE_CLASS_REGISTER
                                                              : STORAGE_CLASS_NONE;
    advance_token();
    return kind;
}

//
// type_specifier
//     : VOID
//     | CHAR
//     | SHORT
//     | INT
//     | LONG
//     | FLOAT
//     | DOUBLE
//     | SIGNED
//     | UNSIGNED
//     | BOOL
//     | COMPLEX
//     | IMAGINARY     /* non-mandated extension */
//     | atomic_type_specifier
//     | struct_or_union_specifier
//     | enum_specifier
//     | TYPEDEF_NAME      /* after it has been defined as such */
//     | coro_frame_specifier  /* vcc extension */
//     ;
// Returns non-NULL value.
//
TypeSpec *parse_type_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    TypeSpec *ts;
    if (current_token == TOKEN_VOID) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_VOID, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_CHAR) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_SHORT) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_SHORT, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_INT) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_LONG) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_LONG, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_FLOAT) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_FLOAT, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_DOUBLE) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_SIGNED) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_SIGNED, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_UNSIGNED) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_UNSIGNED, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_BOOL) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_BOOL, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_COMPLEX) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_COMPLEX, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_IMAGINARY) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_IMAGINARY, __func__, __FILE__, __LINE__);
        advance_token();
    } else if (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN) {
        ts                = new_type_spec(TYPE_SPEC_ATOMIC);
        ts->u.atomic.type = parse_atomic_type_specifier();
    } else if (current_token == TOKEN_CORO_FRAME) {
        ts                = new_type_spec(TYPE_SPEC_CORO_FRAME);
        ts->u.atomic.type = parse_coro_frame_specifier();
    } else if (current_token == TOKEN_CORO_PTR) {
        ts                = new_type_spec(TYPE_SPEC_CORO_FRAME);
        ts->u.atomic.type = parse_coro_ptr_specifier();
    } else if (current_token == TOKEN_STRUCT || current_token == TOKEN_UNION) {
        ts = parse_struct_or_union_specifier();
    } else if (current_token == TOKEN_ENUM) {
        ts = parse_enum_specifier();
    } else if (current_token == TOKEN_TYPEDEF_NAME) {
        ts                      = new_type_spec(TYPE_SPEC_TYPEDEF_NAME);
        ts->u.typedef_name.name = xstrdup(current_lexeme);
        advance_token();
    } else {
        fatal_error("expected a type %s", parser_where());
    }
    return ts;
}

//
// struct_or_union_specifier
//     : struct_or_union '{' struct_declaration_list '}'
//     | struct_or_union IDENTIFIER '{' struct_declaration_list '}'
//     | struct_or_union IDENTIFIER
//     ;
// struct_or_union
//     : STRUCT
//     | UNION
//     ;
//
TypeSpec *parse_struct_or_union_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token != TOKEN_STRUCT && current_token != TOKEN_UNION) {
        internal_error("expected struct or union");
    }
    TypeSpec *ts =
        new_type_spec(current_token == TOKEN_STRUCT ? TYPE_SPEC_STRUCT : TYPE_SPEC_UNION);
    advance_token();
    // Tags are a name space of their own (C11 §6.2.3): the tag may also be a typedef
    // name, as in `typedef struct T T; struct T { ... };`.
    if (current_token == TOKEN_IDENTIFIER || current_token == TOKEN_TYPEDEF_NAME) {
        ts->u.struct_spec.name = xstrdup(current_lexeme);
        advance_token();
    }
    if (current_token == TOKEN_LBRACE) {
        advance_token();
        ts->u.struct_spec.fields = parse_struct_declaration_list();
        expect_token(TOKEN_RBRACE);
    }
    return ts;
}

//
// struct_declaration_list
//     : struct_declaration
//     | struct_declaration_list struct_declaration
//     ;
//
Field *parse_struct_declaration_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Field *fields = NULL, **fields_tail = &fields;
    do {
        //
        // A single struct_declaration yields one field per comma-separated
        // declarator, so append its whole chain, not just its head.
        //
        *fields_tail = parse_struct_declaration();
        while (*fields_tail) {
            fields_tail = &(*fields_tail)->next;
        }
    } while (current_token_is_not(TOKEN_RBRACE));
    return fields;
}

//
// struct_declaration
//     : specifier_qualifier_list ';'  /* for anonymous struct/union */
//     | specifier_qualifier_list struct_declarator_list ';'
//     | static_assert_declaration
//     ;
//
Field *parse_struct_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {
        Declaration *sa                 = parse_static_assert_declaration();
        Field *field                    = new_field(FIELD_STATIC_ASSERT);
        field->u.static_assrt.condition = sa->u.static_assrt.condition;
        field->u.static_assrt.message   = sa->u.static_assrt.message;
        xfree(sa); /* free shell only; condition+message now owned by field */
        return field;
    }

    /* Parse specifier_qualifier_list */
    TypeQualifier *qualifiers = NULL;
    AlignmentSpec *align      = NULL;
    TypeSpec *type_specs      = parse_specifier_qualifier_list(&qualifiers, &align);

    /* Construct base Type from type_specs (simplified to first basic type) */
    Type *base_type = fuse_type_specifiers(type_specs);
    free_type_spec(type_specs);
    base_type->qualifiers = qualifiers;

    /* Parse struct_declarator_list */
    Field *fields = NULL, **fields_tail = &fields;
    for (;;) {
        Field *field               = new_field(FIELD_MEMBER);
        field->u.member.type       = clone_type(base_type, __func__, __FILE__, __LINE__);
        field->u.member.align_spec = clone_alignment_spec(align);

        if (current_token != TOKEN_COLON && current_token != TOKEN_SEMICOLON) {
            Declarator *declarator = parse_declarator();
            field->u.member.name   = declarator->name;
            declarator->name       = NULL;
            if (declarator->pointers) {
                field->u.member.type =
                    type_apply_pointers(field->u.member.type, declarator->pointers);
            }
            if (declarator->suffixes) {
                field->u.member.type =
                    type_apply_suffixes(field->u.member.type, declarator->suffixes);
            }
            free_declarator(declarator);
        }
        if (current_token == TOKEN_COLON) {
            advance_token();
            field->u.member.bitfield = parse_constant_expression();
        }

        /* A member with a scalar/basic type needs a declarator; only an
         * anonymous bitfield (`int : 4;`) or an anonymous struct/union member
         * (`struct { ... };`) may omit it.  Reject e.g. `int;`. */
        if (!field->u.member.name && !field->u.member.bitfield &&
            field->u.member.type->kind != TYPE_STRUCT && field->u.member.type->kind != TYPE_UNION) {
            fatal_error("member declaration does not declare anything");
        }

        *fields_tail = field;
        fields_tail  = &field->next;

        if (current_token == TOKEN_SEMICOLON) {
            break;
        }
        expect_token(TOKEN_COMMA);
    }
    expect_token(TOKEN_SEMICOLON);
    free_type(base_type);
    free_alignment_spec(align);
    return fields;
}

//
// specifier_qualifier_list
//     : type_specifier specifier_qualifier_list
//     | alignment_specifier specifier_qualifier_list   (a member only: `align` non-NULL)
//     | type_specifier
//     | type_qualifier specifier_qualifier_list
//     | type_qualifier
//     ;
// Returns non-NULL value.
//
TypeSpec *parse_specifier_qualifier_list(TypeQualifier **qualifiers, AlignmentSpec **align)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    TypeSpec *type_specs = NULL;
    *qualifiers          = NULL;

    while (1) {
        if (current_token == TOKEN_ALIGNAS && align) {
            parse_one_alignment_specifier(align);
        } else if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                   current_token == TOKEN_VOLATILE ||
                   (current_token == TOKEN_ATOMIC && next_token() != TOKEN_LPAREN)) {
            TypeQualifier *q = parse_type_qualifier();
            append_list(qualifiers, q);
        } else if (is_type_specifier(current_token) || is_type_qualifier(current_token) ||
                   (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN)) {
            TypeSpec *ts = parse_type_specifier();
            append_list(&type_specs, ts);
        } else {
            break; /* End of specifier_qualifier_list */
        }
    }
    if (!type_specs) {
        fatal_error("expected a type %s", parser_where());
    }
    return type_specs;
}

//
// enum_specifier
//     : ENUM '{' enumerator_list '}'
//     | ENUM '{' enumerator_list ',' '}'
//     | ENUM IDENTIFIER '{' enumerator_list '}'
//     | ENUM IDENTIFIER '{' enumerator_list ',' '}'
//     | ENUM IDENTIFIER
//     ;
//
TypeSpec *parse_enum_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ENUM);
    TypeSpec *ts = new_type_spec(TYPE_SPEC_ENUM);
    if (current_token == TOKEN_IDENTIFIER || current_token == TOKEN_TYPEDEF_NAME) {
        ts->u.enum_spec.name = xstrdup(current_lexeme);
        advance_token();
    }
    if (current_token == TOKEN_LBRACE) {
        advance_token();
        ts->u.enum_spec.enumerators = parse_enumerator_list();
        if (current_token == TOKEN_COMMA)
            advance_token();
        expect_token(TOKEN_RBRACE);
    }
    return ts;
}

//
// enumerator_list
//     : enumerator
//     | enumerator_list ',' enumerator
//     ;
//
Enumerator *parse_enumerator_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Enumerator *enumr = parse_enumerator();
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_RBRACE) {
        advance_token();
        enumr->next = parse_enumerator_list();
    }
    return enumr;
}

//
// enumerator  /* identifiers must be flagged as ENUMERATION_CONSTANT */
//     : enumeration_constant '=' constant_expression
//     | enumeration_constant
//     ;
//
Enumerator *parse_enumerator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Ident name = xstrdup(current_lexeme);
    expect_token(TOKEN_IDENTIFIER);
    Expr *value = NULL;
    if (current_token == TOKEN_ASSIGN) {
        advance_token();
        value = parse_constant_expression();
    }
    int token = nametab_find(name);
    if (!token) {
        nametab_define(name, TOKEN_ENUMERATION_CONSTANT, scope_level);
    } else {
        fatal_error("redefinition of enumerator '%s'", name);
    }
    return new_enumerator(name, value);
}

//
// atomic_type_specifier
//     : ATOMIC '(' type_name ')'
//     ;
//
Type *parse_atomic_type_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ATOMIC);
    expect_token(TOKEN_LPAREN);
    Type *type = parse_type_name();
    expect_token(TOKEN_RPAREN);
    return type;
}

//
// type_qualifier
//     : CONST
//     | RESTRICT
//     | VOLATILE
//     | ATOMIC
//     ;
// Returns non-NULL value.
//
TypeQualifier *parse_type_qualifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (current_token) {
    case TOKEN_CONST:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_CONST);
    case TOKEN_RESTRICT:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_RESTRICT);
    case TOKEN_VOLATILE:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_VOLATILE);
    case TOKEN_ATOMIC:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_ATOMIC);
    default:
        internal_error("expected type qualifier");
    }
}

//
// function_specifier
//     : INLINE
//     | NORETURN
//     | CORO '(' type_name ')'   /* vcc extension */
//     ;
//
FunctionSpec *parse_function_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_CORO) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        FunctionSpec *fs = new_function_spec(FUNC_SPEC_CORO);
        fs->yield_type   = parse_type_name();
        expect_token(TOKEN_RPAREN);
        return fs;
    }
    FunctionSpecKind kind = current_token == TOKEN_INLINE ? FUNC_SPEC_INLINE : FUNC_SPEC_NORETURN;
    advance_token();
    return new_function_spec(kind);
}

//
// coro_frame_specifier (vcc extension)
//     : CORO_FRAME '(' type_name ',' type_name ')'
//     ;
// The frame of a coroutine yielding Y and returning T: a struct with the tag
// __co_frame that is never defined, so only pointers to it exist.  Two frame types
// are the same type when their Y and T are (compatible_type).
//
Type *parse_coro_frame_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_CORO_FRAME);
    expect_token(TOKEN_LPAREN);
    Type *frame                   = new_type(TYPE_STRUCT, __func__, __FILE__, __LINE__);
    frame->u.struct_t.name        = xstrdup("__co_frame");
    frame->u.struct_t.frame_yield = parse_type_name();
    expect_token(TOKEN_COMMA);
    frame->u.struct_t.frame_result = parse_type_name();
    expect_token(TOKEN_RPAREN);
    return frame;
}

//
// coro_ptr_specifier (vcc extension)
//     : CORO_PTR '(' type_name ',' type_name ')'
//     ;
// A pointer to a coroutine yielding Y and returning T that takes (void) or (void *):
// a pointer to a struct tagged __co_desc, never defined, which holds Y and T as a frame
// type does.  It points at the coroutine's descriptor.
//
Type *parse_coro_ptr_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_CORO_PTR);
    expect_token(TOKEN_LPAREN);
    Type *desc                   = new_type(TYPE_STRUCT, __func__, __FILE__, __LINE__);
    desc->u.struct_t.name        = xstrdup("__co_desc");
    desc->u.struct_t.frame_yield = parse_type_name();
    expect_token(TOKEN_COMMA);
    desc->u.struct_t.frame_result = parse_type_name();
    expect_token(TOKEN_RPAREN);
    Type *ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
    ptr->u.pointer.target = desc;
    return ptr;
}

//
// alignment_specifier
//     : ALIGNAS '(' type_name ')'
//     | ALIGNAS '(' constant_expression ')'
//     ;
//
// Parse an _Alignas into *slot.  Several in one declaration are not supported (the
// strictest would win), and are rejected rather than mishandled.
void parse_one_alignment_specifier(AlignmentSpec **slot)
{
    if (*slot)
        fatal_error("more than one '_Alignas' in a declaration is not supported");
    *slot = parse_alignment_specifier();
}

AlignmentSpec *parse_alignment_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ALIGNAS);
    expect_token(TOKEN_LPAREN);
    AlignmentSpec *as;
    if (is_type_specifier(current_token)) {
        as         = new_alignment_spec(ALIGN_SPEC_TYPE);
        as->u.type = parse_type_name();
    } else {
        as         = new_alignment_spec(ALIGN_SPEC_EXPR);
        as->u.expr = parse_constant_expression();
    }
    expect_token(TOKEN_RPAREN);
    return as;
}

//
// declarator
//    : pointer direct_declarator
//    | direct_declarator
//    ;
//
Declarator *parse_declarator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Pointer *pointers = NULL;
    if (current_token == TOKEN_STAR) {
        pointers = parse_pointer();
    }
    Declarator *decl = parse_direct_declarator();
    if (pointers) {
        decl->pointers = pointers;
    }
    return decl;
}

//
// direct_declarator
//     : IDENTIFIER
//     | '(' declarator ')'
//     | direct_declarator '[' ']'
//     | direct_declarator '[' '*' ']'
//     | direct_declarator '[' STATIC type_qualifier_list assignment_expression ']'
//     | direct_declarator '[' STATIC assignment_expression ']'
//     | direct_declarator '[' type_qualifier_list '*' ']'
//     | direct_declarator '[' type_qualifier_list STATIC assignment_expression ']'
//     | direct_declarator '[' type_qualifier_list assignment_expression ']'
//     | direct_declarator '[' type_qualifier_list ']'
//     | direct_declarator '[' assignment_expression ']'
//     | direct_declarator '(' parameter_type_list ')'
//     | direct_declarator '(' ')'
//     | direct_declarator '(' identifier_list ')'
//     ;
//
Declarator *parse_direct_declarator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Declarator *decl;
    if (current_token == TOKEN_IDENTIFIER ||
        (current_token == TOKEN_TYPEDEF_NAME && typedef_redeclaration)) {
        decl       = new_declarator();
        decl->name = xstrdup(current_lexeme);
        decl->loc  = diag_loc;
        advance_token();
    } else if (current_token == TOKEN_LPAREN) {
        advance_token();
        decl = parse_declarator();
        expect_token(TOKEN_RPAREN);
        // Parentheses change precedence: outer suffixes (e.g. '()') bind tighter
        // than inner pointers (e.g. '*'). Convert inner pointers to SUFFIX_POINTER
        // so type_apply_suffixes applies outer suffixes before the pointer.
        if (decl->pointers) {
            DeclaratorSuffix *ptr_suffix   = new_declarator_suffix(SUFFIX_POINTER);
            ptr_suffix->u.pointer.pointers = decl->pointers;
            ptr_suffix->u.pointer.suffix   = decl->suffixes;
            decl->pointers                 = NULL;
            decl->suffixes                 = ptr_suffix;
        }
    } else {
        fatal_error("expected an identifier or '(' %s", parser_where());
    }
    while (1) {
        DeclaratorSuffix *suffix = NULL;

        if (current_token == TOKEN_LBRACKET) {
            advance_token();
            suffix = new_declarator_suffix(SUFFIX_ARRAY);
            if (current_token == TOKEN_STATIC) {
                advance_token();
                suffix->u.array.is_static = true;
            }
            TypeQualifier *qualifiers = NULL;
            if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                current_token == TOKEN_VOLATILE ||
                (current_token == TOKEN_ATOMIC && next_token() != TOKEN_LPAREN)) {
                qualifiers = parse_type_qualifier_list();
            }
            Expr *size = NULL;
            if (current_token == TOKEN_STAR) {
                advance_token();
            } else if (current_token_is_not(TOKEN_RBRACKET)) {
                size = parse_assignment_expression();
            }
            expect_token(TOKEN_RBRACKET);
            suffix->u.array.qualifiers = qualifiers;
            suffix->u.array.size       = size;
        } else if (current_token == TOKEN_LPAREN) {
            advance_token();
            suffix = new_declarator_suffix(SUFFIX_FUNCTION);
            if (current_token_is_not(TOKEN_RPAREN)) {
                suffix->u.function.params = parse_parameter_type_list(&suffix->u.function.variadic);
            }
            expect_token(TOKEN_RPAREN);
        } else {
            break;
        }
        append_list(&decl->suffixes, suffix);
    }
    return decl;
}

//
// pointer
//     : '*' type_qualifier_list pointer
//     | '*' type_qualifier_list
//     | '*' pointer
//     | '*'
//     ;
//
Pointer *parse_pointer()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Pointer *pointers = NULL, **pointers_tail = &pointers;
    while (current_token == TOKEN_STAR) {
        Pointer *p = new_pointer();
        advance_token();
        p->qualifiers  = parse_type_qualifier_list();
        *pointers_tail = p;
        pointers_tail  = &p->next;
    }
    return pointers;
}

//
// type_qualifier_list
//     : type_qualifier
//     | type_qualifier_list type_qualifier
//     ;
//
TypeQualifier *parse_type_qualifier_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    TypeQualifier *qualifiers = NULL, **qualifiers_tail = &qualifiers;
    while (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
           current_token == TOKEN_VOLATILE ||
           (current_token == TOKEN_ATOMIC && next_token() != TOKEN_LPAREN)) {
        TypeQualifier *q = new_type_qualifier(TYPE_QUALIFIER_CONST); /* Default */
        switch (current_token) {
        case TOKEN_CONST:
            q->kind = TYPE_QUALIFIER_CONST;
            break;
        case TOKEN_RESTRICT:
            q->kind = TYPE_QUALIFIER_RESTRICT;
            break;
        case TOKEN_VOLATILE:
            q->kind = TYPE_QUALIFIER_VOLATILE;
            break;
        case TOKEN_ATOMIC:
            q->kind = TYPE_QUALIFIER_ATOMIC;
            break;
        default:
            return NULL; /* Unreachable */
        }
        advance_token();
        *qualifiers_tail = q;
        qualifiers_tail  = &q->next;
    }
    return qualifiers;
}

//
// parameter_type_list
//     : parameter_list ',' ELLIPSIS
//     | parameter_list
//     ;
// Return a linked list of parameters.
// Return NULL for empty parameter list.
//
Param *parse_parameter_type_list(bool *variadic_flag)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    // Assume non-variadic by default.
    *variadic_flag = false;
    if (current_token == TOKEN_RPAREN) {
        return NULL;
    }
    if (current_token == TOKEN_ELLIPSIS) {
        fatal_error("a variadic function needs a named parameter before '...'");
    }
    Param *params = parse_parameter_list();
    if (current_token == TOKEN_COMMA && next_token() == TOKEN_ELLIPSIS) {
        advance_token();
        advance_token();
        *variadic_flag = true;
    }
    return params;
}

//
// parameter_list
//     : parameter_declaration
//     | parameter_list ',' parameter_declaration
//     ;
//
Param *parse_parameter_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Param *param = parse_parameter_declaration();
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_ELLIPSIS) {
        advance_token();
        param->next = parse_parameter_list();
    }
    return param;
}

//
// abstract_declarator
//     : pointer direct_abstract_declarator
//     | pointer
//     | direct_abstract_declarator
//     ;
//
// direct_abstract_declarator
//     : '(' abstract_declarator ')'
//     | '[' ']'
//     | '[' '*' ']'
//     | '[' STATIC type_qualifier_list assignment_expression ']'
//     | '[' STATIC assignment_expression ']'
//     | '[' type_qualifier_list STATIC assignment_expression ']'
//     | '[' type_qualifier_list assignment_expression ']'
//     | '[' type_qualifier_list ']'
//     | '[' assignment_expression ']'
//     | direct_abstract_declarator '[' ']'
//     | direct_abstract_declarator '[' '*' ']'
//     | direct_abstract_declarator '[' STATIC type_qualifier_list assignment_expression ']'
//     | direct_abstract_declarator '[' STATIC assignment_expression ']'
//     | direct_abstract_declarator '[' type_qualifier_list assignment_expression ']'
//     | direct_abstract_declarator '[' type_qualifier_list STATIC assignment_expression ']'
//     | direct_abstract_declarator '[' type_qualifier_list ']'
//     | direct_abstract_declarator '[' assignment_expression ']'
//     | '(' ')'
//     | '(' parameter_type_list ')'
//     | direct_abstract_declarator '(' ')'
//     | direct_abstract_declarator '(' parameter_type_list ')'
//     ;
//
DeclaratorSuffix *parse_direct_abstract_declarator(Ident *name_out)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    DeclaratorSuffix *suffix = NULL;
    DeclaratorSuffix **tail  = &suffix; // Pointer to the last suffix's next field

    while (1) {
        if (name_out && current_token == TOKEN_IDENTIFIER) {
            // Pass name to caller.
            *name_out = xstrdup(current_lexeme);
            advance_token();
        } else if (current_token == TOKEN_LPAREN) {
            // Handle '(' abstract_declarator ')' or '(' parameter_type_list ')' or '(' ')'
            advance_token(); // Consume '('
            if (current_token == TOKEN_RPAREN) {
                // Case: '(' ')'
                advance_token(); // Consume ')'
                DeclaratorSuffix *new_suffix    = new_declarator_suffix(SUFFIX_FUNCTION);
                new_suffix->u.function.params   = NULL;
                new_suffix->u.function.variadic = false;
                *tail                           = new_suffix;
                tail                            = &new_suffix->next;
            } else if (current_token == TOKEN_STAR || current_token == TOKEN_LPAREN ||
                       current_token == TOKEN_LBRACKET ||
                       (name_out && current_token == TOKEN_IDENTIFIER)) {
                // Case: '(' abstract_declarator ')'.  A following '(' begins a
                // nested abstract declarator, a following '[' begins a nested
                // *array* abstract declarator (e.g. the group "([3][4])"), and a
                // following plain identifier (only when a name is still expected)
                // is the declared name wrapped in redundant parens, e.g. the
                // parameter "int(*(p))".  A real parameter list would start with a
                // type specifier or a TOKEN_TYPEDEF_NAME, never '(', '[', or a
                // plain identifier.  parse_pointer() yields NULL for these, so the
                // extra parens act as pure grouping.
                DeclaratorSuffix *new_suffix   = new_declarator_suffix(SUFFIX_POINTER);
                new_suffix->u.pointer.pointers = parse_pointer();
                new_suffix->u.pointer.suffix   = parse_direct_abstract_declarator(name_out);
                expect_token(TOKEN_RPAREN); // Consume ')'
                *tail = new_suffix;
                tail  = &new_suffix->next;
            } else if (current_token == TOKEN_ELLIPSIS) {
                fatal_error("a variadic function needs a named parameter before '...'");
            } else {
                // Case: '(' parameter_type_list ')'
                DeclaratorSuffix *new_suffix = new_declarator_suffix(SUFFIX_FUNCTION);
                new_suffix->u.function.params =
                    parse_parameter_type_list(&new_suffix->u.function.variadic);
                expect_token(TOKEN_RPAREN); // Consume ')'
                *tail = new_suffix;
                tail  = &new_suffix->next;
            }
        } else if (current_token == TOKEN_LBRACKET) {
            // Handle array-related cases
            advance_token(); // Consume '['
            DeclaratorSuffix *new_suffix   = new_declarator_suffix(SUFFIX_ARRAY);
            new_suffix->u.array.size       = NULL;
            new_suffix->u.array.qualifiers = NULL;
            new_suffix->u.array.is_static  = false;

            if (current_token == TOKEN_RBRACKET) {
                // Case: '[' ']'
                advance_token(); // Consume ']'
            } else if (current_token == TOKEN_STAR) {
                // Case: '[' '*' ']'
                advance_token();                 // Consume '*'
                expect_token(TOKEN_RBRACKET);    // Consume ']'
                new_suffix->u.array.size = NULL; // VLA with *
            } else if (current_token == TOKEN_STATIC) {
                // Cases: '[' STATIC ... ']'
                advance_token(); // Consume STATIC
                new_suffix->u.array.is_static = true;
                if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                    current_token == TOKEN_VOLATILE || current_token == TOKEN_ATOMIC) {
                    // Case: '[' STATIC type_qualifier_list assignment_expression ']'
                    new_suffix->u.array.qualifiers = parse_type_qualifier_list();
                } else {
                    // Case: '[' STATIC assignment_expression ']'
                }
                new_suffix->u.array.size = parse_assignment_expression();
                if (!new_suffix->u.array.size) {
                    internal_error("invalid array size");
                }
                expect_token(TOKEN_RBRACKET); // Consume ']'
            } else if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                       current_token == TOKEN_VOLATILE || current_token == TOKEN_ATOMIC) {
                // Cases: '[' type_qualifier_list ... ']'
                new_suffix->u.array.qualifiers = parse_type_qualifier_list();
                if (current_token == TOKEN_STATIC) {
                    // Case: '[' type_qualifier_list STATIC assignment_expression ']'
                    advance_token(); // Consume STATIC
                    new_suffix->u.array.is_static = true;
                    new_suffix->u.array.size      = parse_assignment_expression();
                    if (!new_suffix->u.array.size) {
                        internal_error("invalid array size");
                    }
                } else {
                    // Case: '[' type_qualifier_list assignment_expression ']'
                    //    or '[' type_qualifier_list ']'
                    if (current_token_is_not(TOKEN_RBRACKET)) {
                        new_suffix->u.array.size = parse_assignment_expression();
                        if (!new_suffix->u.array.size) {
                            internal_error("invalid array size");
                        }
                    }
                }
                expect_token(TOKEN_RBRACKET); // Consume ']'
            } else {
                // Case: '[' assignment_expression ']'
                new_suffix->u.array.size = parse_assignment_expression();
                if (!new_suffix->u.array.size) {
                    internal_error("invalid array size");
                }
                expect_token(TOKEN_RBRACKET); // Consume ']'
            }
            *tail = new_suffix;
            tail  = &new_suffix->next;
        } else {
            // No more suffixes to parse
            break;
        }
    }
    return suffix;
}

//
// parameter_declaration
//     : declaration_specifiers declarator
//     | declaration_specifiers abstract_declarator
//     | declaration_specifiers
//     ;
//
Param *parse_parameter_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Param *param = new_param();

    /* Parse declaration_specifiers */
    param->specifiers = parse_declaration_specifiers(&param->type);

    /* The only storage-class specifier permitted on a function parameter is
     * register (C11 §6.7.6.3p2) — reject "f(extern int i)", "f(static int i)",
     * etc., but allow "f(register int i)". */
    if (param->specifiers && param->specifiers->storage != STORAGE_CLASS_NONE &&
        param->specifiers->storage != STORAGE_CLASS_REGISTER) {
        fatal_error("a parameter cannot have a storage class");
    }

    /* Check for declarator or abstract_declarator */
    if (current_token == TOKEN_IDENTIFIER) {
        param->name = xstrdup(current_lexeme);
        param->loc  = diag_loc;
        advance_token();
    }
    if (current_token == TOKEN_STAR || current_token == TOKEN_LBRACKET ||
        current_token == TOKEN_LPAREN) {
        Pointer *pointers = parse_pointer();
        DeclaratorSuffix *suffixes =
            parse_direct_abstract_declarator(param->name ? NULL : &param->name);

        /* Apply pointers and suffixes */
        param->type = type_apply_suffixes(type_apply_pointers(param->type, pointers), suffixes);
        free_pointer(pointers);
        free_declarator_suffix(suffixes);
    }
    return param;
}

//
// type_name : specifier_qualifier_list abstract_declarator
//           | specifier_qualifier_list
//           ;
// Returns non-NULL value.
//
Type *parse_type_name()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    TypeQualifier *qualifiers = NULL;
    TypeSpec *type_specs      = parse_specifier_qualifier_list(&qualifiers, NULL);

    /* Construct base Type from type_specs (simplified to first basic type) */
    Type *base_type = fuse_type_specifiers(type_specs);
    free_type_spec(type_specs);
    base_type->qualifiers = qualifiers;

    /* Parse optional abstract_declarator */
    Pointer *pointers          = NULL;
    DeclaratorSuffix *suffixes = NULL;
    if (current_token == TOKEN_STAR || current_token == TOKEN_LPAREN ||
        current_token == TOKEN_LBRACKET) {
        // Parse abstract_declarator
        pointers = parse_pointer();
        suffixes = parse_direct_abstract_declarator(NULL);
    }

    /* Apply pointers and suffixes to construct the final type */
    Type *type = type_apply_suffixes(type_apply_pointers(base_type, pointers), suffixes);
    free_pointer(pointers);
    free_declarator_suffix(suffixes);
    return type;
}

//
// initializer
//     : '{' initializer_list '}'
//     | '{' initializer_list ',' '}'
//     | assignment_expression
//     ;
//
Initializer *parse_initializer()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_LBRACE) {
        SrcLoc loc = diag_loc; // of the '{'
        advance_token();
        InitItem *items = NULL;
        if (current_token != TOKEN_RBRACE) {
            items = parse_initializer_list();
            if (current_token == TOKEN_COMMA)
                advance_token();
        }
        expect_token(TOKEN_RBRACE);
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->loc         = loc;
        init->u.items     = items;
        return init;
    }
    Initializer *init = new_initializer(INITIALIZER_SINGLE);
    init->u.expr      = parse_assignment_expression();
    return init;
}

//
// initializer_list
//     : designation initializer
//     | initializer
//     | initializer_list ',' designation initializer
//     | initializer_list ',' initializer
//     ;
//
InitItem *parse_initializer_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designation = NULL;
    if (current_token == TOKEN_LBRACKET || current_token == TOKEN_DOT) {
        designation = parse_designation();
    }
    Initializer *init = parse_initializer();
    InitItem *item    = new_init_item(designation, init);
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_RBRACE) {
        advance_token();
        item->next = parse_initializer_list();
    }
    return item;
}

//
// designation
//     : designator_list '='
//     ;
//
Designator *parse_designation()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designators = parse_designator_list();
    expect_token(TOKEN_ASSIGN);
    return designators;
}

//
// designator_list
//     : designator
//     | designator_list designator
//     ;
//
Designator *parse_designator_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designator = parse_designator();
    if (current_token == TOKEN_LBRACKET || current_token == TOKEN_DOT) {
        designator->next = parse_designator_list();
    }
    return designator;
}

//
// designator
//     : '[' constant_expression ']'
//     | '.' IDENTIFIER
//     ;
//
Designator *parse_designator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_LBRACKET) {
        advance_token();
        Expr *expr = parse_constant_expression();
        expect_token(TOKEN_RBRACKET);
        Designator *d = new_designator(DESIGNATOR_ARRAY);
        d->u.expr     = expr;
        return d;
    }
    if (current_token != TOKEN_DOT) {
        fatal_error("expected a designator %s", parser_where());
    }
    advance_token();
    Ident name = xstrdup(current_lexeme);
    expect_token(TOKEN_IDENTIFIER);
    Designator *d = new_designator(DESIGNATOR_FIELD);
    d->u.name     = name;
    return d;
}

//
// static_assert_declaration
//     : STATIC_ASSERT '(' constant_expression ',' STRING_LITERAL ')' ';'
//     ;
//
Declaration *parse_static_assert_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    SrcLoc loc = diag_loc; // of the _Static_assert
    expect_token(TOKEN_STATIC_ASSERT);
    expect_token(TOKEN_LPAREN);
    Expr *condition = parse_constant_expression();
    expect_token(TOKEN_COMMA);
    char *message = strip_string_literal_lexeme(current_lexeme);
    expect_token(TOKEN_STRING_LITERAL);
    expect_token(TOKEN_RPAREN);
    expect_token(TOKEN_SEMICOLON);

    Declaration *decl              = new_declaration(DECL_STATIC_ASSERT);
    decl->loc                      = loc;
    decl->u.static_assrt.condition = condition;
    decl->u.static_assrt.message   = message;
    return decl;
}
