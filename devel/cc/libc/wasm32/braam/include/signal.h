/*
 * <signal.h> for Braam.  There are no handlers: a signal is asked for with sig_catch
 * and collected with sig_take (braam.h), where the program parks.  So signal() and
 * raise() are not declared, and a port says what to do at the read or the sleep that
 * gives up with EINTR.
 */
#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <braam.h>

typedef int sig_atomic_t;

#define SIGINT   BRAAM_SIG_INT /* ^C */
#define SIGKILL  BRAAM_SIG_KILL
#define SIGTERM  BRAAM_SIG_TERM /* what kill sends */
#define SIGCONT  BRAAM_SIG_CONT
#define SIGTSTP  BRAAM_SIG_TSTP
#define SIGWINCH BRAAM_SIG_WINCH /* the terminal changed shape */

#endif /* _SIGNAL_H */
