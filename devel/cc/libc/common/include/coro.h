/*
 * <coro.h> — short names for vcc's extensions of C: `defer` on every target, and
 * coroutines on every target but BESM-6, where the compiler predefines
 * __vcc_coroutines__ (docs/Coroutines_in_C.md).  Without this header the reserved spellings still
 * work: _Defer, _Coro(Y), _Yield, _Await, _Coro_frame(Y, T), _Coro_ptr(Y, T) and
 * __co_init ... __co_alignof.
 */
#ifndef _CORO_H
#define _CORO_H

#define defer _Defer

#define coro(Y)        _Coro(Y)
#define yield          _Yield
#define await          _Await
#define co_frame(Y, T) _Coro_frame(Y, T)
#define coro_ptr(Y, T) _Coro_ptr(Y, T)

#define co_init    __co_init
#define co_alloca  __co_alloca
#define co_resume  __co_resume
#define co_cancel  __co_cancel
#define co_destroy __co_destroy
#define co_done    __co_done
#define co_value   __co_value
#define co_result  __co_result
#define co_sizeof  __co_sizeof
#define co_alignof __co_alignof

/* What co_resume, co_cancel and co_destroy return. */
typedef enum { CO_SUSPENDED, CO_DONE } co_status;

/* What yield returns: how the coroutine was resumed. */
typedef enum { CO_CONTINUE, CO_CANCEL } co_signal;

#endif /* _CORO_H */
