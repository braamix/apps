/*
 * <fenv.h> — floating-point environment (C11 §7.6).
 *
 * The floating-point environment is degenerate: a single mode and no sticky
 * flags (BESM-6 has no IEEE flags or rounding-mode register; on RISC-V they are
 * not exposed yet).  The surface is provided for source portability (TODO).
 */
#ifndef _FENV_H
#define _FENV_H

typedef int fexcept_t;

typedef struct {
    int __mode;
} fenv_t;

/* No exception flags are raised. */
#define FE_ALL_EXCEPT 0

/* Single rounding mode: round to nearest. */
#define FE_TONEAREST 0

#define FE_DFL_ENV ((const fenv_t *)0)

int feclearexcept(int excepts);
int fetestexcept(int excepts);
int feraiseexcept(int excepts);
int fegetexceptflag(fexcept_t *flagp, int excepts);
int fesetexceptflag(const fexcept_t *flagp, int excepts);

int fegetround(void);
int fesetround(int round);

int fegetenv(fenv_t *envp);
int fesetenv(const fenv_t *envp);
int feholdexcept(fenv_t *envp);
int feupdateenv(const fenv_t *envp);

#endif /* _FENV_H */
