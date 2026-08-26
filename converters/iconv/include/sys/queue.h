/* The BSD intrusive lists, in the subset citrus uses: a singly-linked tail
 * queue, a doubly-linked one, and a plain list. Pointer arithmetic only, no
 * allocation, so the definitions are the same ones FreeBSD ships less the
 * debugging and the variants nothing here names. */
#ifndef _SYS_QUEUE_H_
#define _SYS_QUEUE_H_

/* -- singly-linked tail queue ------------------------------------------- */

#define STAILQ_HEAD(name, type)  \
    struct name {                \
        struct type *stqh_first; \
        struct type **stqh_last; \
    }

#define STAILQ_ENTRY(type)      \
    struct {                    \
        struct type *stqe_next; \
    }

#define STAILQ_INIT(head)                         \
    do {                                          \
        (head)->stqh_first = nullptr;             \
        (head)->stqh_last  = &(head)->stqh_first; \
    } while (0)

#define STAILQ_FIRST(head)      ((head)->stqh_first)
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)

#define STAILQ_FOREACH(var, head, field) \
    for ((var) = STAILQ_FIRST(head); (var); (var) = STAILQ_NEXT(var, field))

#define STAILQ_INSERT_TAIL(head, elm, field)                \
    do {                                                    \
        STAILQ_NEXT(elm, field) = nullptr;                  \
        *(head)->stqh_last      = (elm);                    \
        (head)->stqh_last       = &STAILQ_NEXT(elm, field); \
    } while (0)

#define STAILQ_REMOVE_HEAD(head, field)                                               \
    do {                                                                              \
        if (((head)->stqh_first = STAILQ_NEXT((head)->stqh_first, field)) == nullptr) \
            (head)->stqh_last = &(head)->stqh_first;                                  \
    } while (0)

/* -- doubly-linked tail queue ------------------------------------------- */

#define TAILQ_HEAD(name, type)  \
    struct name {               \
        struct type *tqh_first; \
        struct type **tqh_last; \
    }

#define TAILQ_ENTRY(type)       \
    struct {                    \
        struct type *tqe_next;  \
        struct type **tqe_prev; \
    }

#define TAILQ_INIT(head)                        \
    do {                                        \
        (head)->tqh_first = nullptr;            \
        (head)->tqh_last  = &(head)->tqh_first; \
    } while (0)

#define TAILQ_FIRST(head)      ((head)->tqh_first)
#define TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)
#define TAILQ_EMPTY(head)      ((head)->tqh_first == nullptr)

/* headname is the struct tag, which is how the last element is reached
 * without a back pointer in the head. */
#define TAILQ_LAST(head, headname) (*(((struct headname *)((head)->tqh_last))->tqh_last))

#define TAILQ_PREV(elm, headname, field) (*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))

#define TAILQ_FOREACH(var, head, field) \
    for ((var) = TAILQ_FIRST(head); (var); (var) = TAILQ_NEXT(var, field))

#define TAILQ_FOREACH_SAFE(var, head, field, tvar) \
    for ((var) = TAILQ_FIRST(head); (var) && ((tvar) = TAILQ_NEXT(var, field), 1); (var) = (tvar))

#define TAILQ_INSERT_TAIL(head, elm, field)               \
    do {                                                  \
        TAILQ_NEXT(elm, field) = nullptr;                 \
        (elm)->field.tqe_prev  = (head)->tqh_last;        \
        *(head)->tqh_last      = (elm);                   \
        (head)->tqh_last       = &TAILQ_NEXT(elm, field); \
    } while (0)

#define TAILQ_INSERT_BEFORE(listelm, elm, field)                \
    do {                                                        \
        (elm)->field.tqe_prev      = (listelm)->field.tqe_prev; \
        TAILQ_NEXT(elm, field)     = (listelm);                 \
        *(listelm)->field.tqe_prev = (elm);                     \
        (listelm)->field.tqe_prev  = &TAILQ_NEXT(elm, field);   \
    } while (0)

#define TAILQ_REMOVE(head, elm, field)                                      \
    do {                                                                    \
        if ((TAILQ_NEXT(elm, field)) != nullptr)                            \
            TAILQ_NEXT(elm, field)->field.tqe_prev = (elm)->field.tqe_prev; \
        else                                                                \
            (head)->tqh_last = (elm)->field.tqe_prev;                       \
        *(elm)->field.tqe_prev = TAILQ_NEXT(elm, field);                    \
    } while (0)

/* -- list ---------------------------------------------------------------- */

#define LIST_HEAD(name, type)  \
    struct name {              \
        struct type *lh_first; \
    }

#define LIST_ENTRY(type)       \
    struct {                   \
        struct type *le_next;  \
        struct type **le_prev; \
    }

#define LIST_INIT(head)             \
    do {                            \
        (head)->lh_first = nullptr; \
    } while (0)

#define LIST_FIRST(head)      ((head)->lh_first)
#define LIST_NEXT(elm, field) ((elm)->field.le_next)

#define LIST_FOREACH(var, head, field) \
    for ((var) = LIST_FIRST(head); (var); (var) = LIST_NEXT(var, field))

#define LIST_INSERT_HEAD(head, elm, field)                            \
    do {                                                              \
        if ((LIST_NEXT(elm, field) = (head)->lh_first) != nullptr)    \
            (head)->lh_first->field.le_prev = &LIST_NEXT(elm, field); \
        (head)->lh_first     = (elm);                                 \
        (elm)->field.le_prev = &(head)->lh_first;                     \
    } while (0)

#define LIST_REMOVE(elm, field)                                          \
    do {                                                                 \
        if (LIST_NEXT(elm, field) != nullptr)                            \
            LIST_NEXT(elm, field)->field.le_prev = (elm)->field.le_prev; \
        *(elm)->field.le_prev = LIST_NEXT(elm, field);                   \
    } while (0)

#endif
