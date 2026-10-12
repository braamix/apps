//
// Where labels stand with respect to `defer` (docs/Coroutines_in_C.md, section 1).
//
#ifndef DEFER_H
#define DEFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "string_map.h"

// One block on the way from the root down to a position, and the number of defers
// registered in it before that position.  A block is a compound statement, or the
// substatement of a selection, iteration or defer statement that is not a compound
// one (a block of its own in C11, §6.8.4p3, §6.8.5p5); `key` is that statement.
typedef struct {
    const Stmt *key;
    int count;
} DeferScope;

// A position: the blocks from the root down.  The root is the function body, or the
// innermost deferred statement holding the position, which nothing may leave or enter.
typedef struct {
    int depth;
    DeferScope *scopes;
    const Stmt *defer; // the innermost deferred statement holding it, or NULL
} DeferPos;

// The position of every label of a function body, by name (DeferPos * values).
void defer_collect_labels(const Stmt *body, StringMap *labels);
void defer_free_labels(StringMap *labels);

#ifdef __cplusplus
}
#endif

#endif
