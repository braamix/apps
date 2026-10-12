/*
 * <spawn.h> for Braam: posix_spawn over Sys::Spawn.  The child's standard
 * descriptors are this process's, and there are no file actions or attributes to
 * change that: both must be NULL, or the answer is ENOTSUP.  The kernel looks a name
 * without a slash up along PATH, so posix_spawn and posix_spawnp are one call; the
 * child's argv[0] is path.  envp == environ (or NULL) lets the child inherit.
 * Each returns 0, or an error number, and does not set errno.
 */
#ifndef _SPAWN_H
#define _SPAWN_H

#include <braam.h>
#include <sys/types.h>

typedef struct __posix_spawn_file_actions posix_spawn_file_actions_t;
typedef struct __posix_spawnattr posix_spawnattr_t;

coro(braam_call *) int posix_spawn(pid_t *pid, const char *path,
                                   const posix_spawn_file_actions_t *actions,
                                   const posix_spawnattr_t *attr, char *const argv[],
                                   char *const envp[]);
coro(braam_call *) int posix_spawnp(pid_t *pid, const char *file,
                                    const posix_spawn_file_actions_t *actions,
                                    const posix_spawnattr_t *attr, char *const argv[],
                                    char *const envp[]);

extern char **environ;

#endif /* _SPAWN_H */
