/*
 * <poll.h> for Braam: poll, a coroutine yielding braam_call * (Sys::Poll).  Pipes, the
 * standard streams behind them and files answer; a descriptor that waits on the host
 * (a socket, a fetch) is ENOTSUP.  At most BRAAM_POLL_MAX descriptors.
 */
#ifndef _POLL_H
#define _POLL_H

#include <braam.h>

struct pollfd {
    int fd;
    short events;  /* POLLIN, POLLOUT */
    short revents; /* those ready, and POLLHUP whether asked or not */
};

typedef unsigned nfds_t;

#define POLLIN  BRAAM_POLL_IN
#define POLLOUT BRAAM_POLL_OUT
#define POLLHUP BRAAM_POLL_HUP

/* How many have revents, 0 at the timeout (milliseconds, negative for ever), or -1
   with errno set (EINTR for a signal asked for). */
coro(braam_call *) int poll(struct pollfd *fds, nfds_t n, int timeout);

#endif /* _POLL_H */
