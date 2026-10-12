/*
 * <sys/wait.h> for Braam: waitpid over Sys::Wait.  A child's end is always an exit
 * status, 130 for an interrupt and 132 for a trap among them, so WIFEXITED is always
 * true.  options must be 0.
 */
#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <braam.h>
#include <sys/types.h>

#define WIFEXITED(s)   1
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) 0
#define WTERMSIG(s)    0

/* pid -1 waits for any child. */
coro(braam_call *) pid_t waitpid(pid_t pid, int *status, int options);
coro(braam_call *) pid_t wait(int *status);

#endif /* _SYS_WAIT_H */
