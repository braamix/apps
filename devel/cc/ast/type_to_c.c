//
// A type written as C, for diagnostics: "const char *", "int (*)(int)", "struct S".
//
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "xalloc.h"

// Concatenate three strings into a new one.
static char *cat3(const char *a, const char *b, const char *c)
{
    size_t la = strlen(a), lb = strlen(b), lc = strlen(c);
    char *s = xalloc(la + lb + lc + 1, __func__, __FILE__, __LINE__);
    memcpy(s, a, la);
    memcpy(s + la, b, lb);
    memcpy(s + la + lb, c, lc + 1);
    return s;
}

// Append text to *s, which is replaced.
static void append(char **s, const char *a, const char *b)
{
    char *t = cat3(*s, a, b);
    xfree(*s);
    *s = t;
}

// The qualifiers, each followed by a space: "const volatile ".
static char *qualifiers(const TypeQualifier *q)
{
    char *s = xstrdup("");
    for (; q; q = q->next) {
        switch (q->kind) {
        case TYPE_QUALIFIER_CONST:
            append(&s, "const", " ");
            break;
        case TYPE_QUALIFIER_RESTRICT:
            append(&s, "restrict", " ");
            break;
        case TYPE_QUALIFIER_VOLATILE:
            append(&s, "volatile", " ");
            break;
        case TYPE_QUALIFIER_ATOMIC:
            append(&s, "_Atomic", " ");
            break;
        }
    }
    return s;
}

// A tag name; the parser names an untagged struct, union or enum "__anon_N".
static const char *tag_name(const char *name)
{
    if (!name || strncmp(name, "__anon_", 7) == 0)
        return "<anonymous>";
    return name;
}

// The bound of an array: a constant, or nothing when it is not one.
static char *array_bound(const Expr *size)
{
    char buf[32];
    if (!size)
        return xstrdup("[]");
    if (size->kind != EXPR_LITERAL || !size->u.literal)
        return xstrdup("[*]");
    const Literal *lit = size->u.literal;
    switch (lit->kind) {
    case LITERAL_INT:
    case LITERAL_LONG:
    case LITERAL_LONG_LONG:
    case LITERAL_CHAR:
        snprintf(buf, sizeof(buf), "[%" PRId64 "]", lit->u.int_val);
        break;
    case LITERAL_UINT:
    case LITERAL_ULONG:
    case LITERAL_ULONG_LONG:
        snprintf(buf, sizeof(buf), "[%" PRIu64 "]", lit->u.uint_val);
        break;
    default:
        return xstrdup("[*]");
    }
    return xstrdup(buf);
}

static char *declare(const Type *t, char *declarator);

// The parameter list of a function type, in parentheses.
static char *parameter_list(const Type *t)
{
    char *s = xstrdup("(");
    for (const Param *p = t->u.function.params; p; p = p->next) {
        char *type = declare(p->type, xstrdup(""));
        append(&s, type, p->next ? ", " : "");
        xfree(type);
    }
    if (t->u.function.variadic)
        append(&s, t->u.function.params ? ", " : "", "...");
    append(&s, ")", "");
    return s;
}

//
// The type t declaring the declarator: the base type, then the declarator wrapped in
// the pointers, arrays and functions that make t, inside out. Takes the declarator.
//
static char *declare(const Type *t, char *declarator)
{
    if (!t)
        return cat3("<unknown type>", "", "");

    char *quals = qualifiers(t->qualifiers);
    char *result;
    switch (t->kind) {
    case TYPE_POINTER: {
        char *pq = qualifiers(t->u.pointer.qualifiers);
        append(&pq, quals, "");
        size_t n = strlen(pq);
        if (n > 0 && declarator[0] == '\0')
            pq[n - 1] = '\0'; // "* const", not "* const "
        char *d = cat3("*", pq, declarator);
        xfree(pq);
        const Type *target = t->u.pointer.target;
        if (target && (target->kind == TYPE_ARRAY || target->kind == TYPE_FUNCTION)) {
            char *wrapped = cat3("(", d, ")");
            xfree(d);
            d = wrapped;
        }
        xfree(declarator);
        xfree(quals);
        return declare(target, d);
    }
    case TYPE_ARRAY: {
        char *bound = array_bound(t->u.array.size);
        char *d     = cat3(declarator, bound, "");
        xfree(bound);
        xfree(declarator);
        xfree(quals);
        return declare(t->u.array.element, d);
    }
    case TYPE_FUNCTION: {
        char *params = parameter_list(t);
        char *d      = cat3(declarator, params, "");
        xfree(params);
        xfree(declarator);
        xfree(quals);
        return declare(t->u.function.return_type, d);
    }
    case TYPE_STRUCT:
    case TYPE_UNION:
        if (t->u.struct_t.frame_yield) {
            char *y = declare(t->u.struct_t.frame_yield, xstrdup(""));
            char *r = declare(t->u.struct_t.frame_result, xstrdup(""));
            result  = cat3("_Coro_frame(", y, ", ");
            append(&result, r, ")");
            xfree(y);
            xfree(r);
        } else {
            result = cat3(t->kind == TYPE_STRUCT ? "struct " : "union ",
                          tag_name(t->u.struct_t.name), "");
        }
        break;
    case TYPE_ENUM:
        result = cat3("enum ", tag_name(t->u.enum_t.name), "");
        break;
    case TYPE_TYPEDEF_NAME:
        result = xstrdup(t->u.typedef_name.name);
        break;
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY: {
        char *base = declare(t->u.complex.base, xstrdup(""));
        result     = cat3(base, t->kind == TYPE_COMPLEX ? " _Complex" : " _Imaginary", "");
        xfree(base);
        break;
    }
    case TYPE_ATOMIC: {
        char *base = declare(t->u.atomic.base, xstrdup(""));
        result     = cat3("_Atomic(", base, ")");
        xfree(base);
        break;
    }
    default:
        result = xstrdup(type_kind_str[t->kind]);
        break;
    }
    char *s = cat3(quals, result, "");
    if (declarator[0]) {
        // "int [3]" and "int (*)(void)", as Clang writes them; "char *" likewise.
        append(&s, " ", declarator);
    }
    xfree(quals);
    xfree(result);
    xfree(declarator);
    return s;
}

char *type_to_c(const Type *type)
{
    return declare(type, xstrdup(""));
}
