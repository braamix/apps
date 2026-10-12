/*
 * <setjmp.h> — non-local jumps (C11 §7.13), wasm32 target.
 *
 * Not implemented, and not implementable on the wasm features this target uses:
 * longjmp needs wasm exception handling or stack switching to unwind frames that
 * live on the engine's own stack.  Declared so that a program which never calls them
 * still compiles; one that does fails to link.
 */
#ifndef _SETJMP_H
#define _SETJMP_H

typedef long long jmp_buf[8];

int setjmp(jmp_buf env);
_Noreturn void longjmp(jmp_buf env, int val);

#endif /* _SETJMP_H */
