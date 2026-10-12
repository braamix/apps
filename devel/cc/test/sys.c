// The C library's half that asks Braam: the environment, the terminal, the clock,
// a new file, a child process and atexit.  sys.mjs holds the lines to what they
// should be.  The working directory is /home, and /tmp/d/up -> .. and
// /tmp/l -> d/up/d are links.
//
//     sys [child status]
//     sys exit
//
// With arguments it is the child: it prints them and exits with the status.  With
// "exit" it prints a line and calls exit(3) from a plain function.
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void bye(void)
{
    printf("atexit 1\n");
}

static void bye2(void)
{
    printf("atexit 2\n");
}

static void leave(void)
{
    printf("before exit\n");
    fprintf(stderr, "on stderr\n");
    exit(3);
}

coro(braam_call *) int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "exit") == 0)
        leave();
    if (argc > 1) {
        printf("child %s %s %s\n", argv[0], argv[1], getenv("SYS_TEST") ? getenv("SYS_TEST") : "-");
        return atoi(argv[1]);
    }

    printf("env %s %s %d\n", getenv("HOME"), getenv("SYS_TEST") ? getenv("SYS_TEST") : "-",
           getenv("NO_SUCH_VARIABLE") == NULL);
    int t0 = await isatty(0), t1 = await isatty(1), t9 = await isatty(9);
    printf("isatty %d %d %d %d\n", t0, t1, t9, errno != 0);

    time_t now    = await time(NULL);
    struct tm *tm = gmtime(&now);
    printf("time %d %d\n", now > 1700000000, tm->tm_year >= 123);
    char buf[64];
    strftime(buf, sizeof buf, "%Y", localtime(&now));
    printf("localtime %d\n", strlen(buf) == 4);

    printf("access %d %d\n", await access("/bin/sh", F_OK), await access("/no/such", F_OK));

    char real[PATH_MAX], *r;
    const char *const paths[] = { "/tmp/../bin//./sh", ".", "/tmp/l",
                                  "/tmp/l/up/d/up",    "/", "/no/such" };
    printf("realpath");
    for (int k = 0; k < 6; k++)
        printf(" %s", (r = await realpath(paths[k], real)) ? r : "-");
    r = await realpath("/tmp/d/up", NULL);
    char link[16];
    ssize_t n = await readlink("/tmp/l", link, sizeof link);
    printf(" %s %.*s %d\n", r, (int)n, link, (int)await readlink("/bin/sh", link, 16));
    free(r);

    char name[] = "/tmp/sysXXXXXX.c";
    int fd      = await mkstemps(name, 2);
    int fd2;
    char again[] = "/tmp/sysXXXXXX";
    fd2          = await mkstemp(again);
    printf("mkstemps %d %d %.8s %s %d\n", fd >= 0, fd2 >= 0, name, name + 14,
           strcmp(name, again) != 0 && strchr(name, 'X') == NULL);
    await close(fd);
    await close(fd2);
    errno      = 0;
    char bad[] = "/tmp/sysXXXX";
    printf("mkstemp bad %d %d\n", await mkstemp(bad), errno == EINVAL);

    await fflush(stdout);
    pid_t pid;
    char *args[] = { "ignored", "7", NULL };
    int err      = await posix_spawn(&pid, "sys", NULL, NULL, args, environ);
    int status   = -1;
    pid_t got    = await waitpid(pid, &status, 0);
    printf("spawn %d %d %d %d\n", err, got == pid, WIFEXITED(status), WEXITSTATUS(status));

    char *env[]   = { "SYS_TEST=given", NULL };
    char *args2[] = { "sys", "0", NULL };
    err           = await posix_spawnp(&pid, "sys", NULL, NULL, args2, env);
    got           = await wait(&status);
    printf("spawnp %d %d %d\n", err, got == pid, WEXITSTATUS(status));

    err = await posix_spawn(&pid, "/no/such/program", NULL, NULL, args2, environ);
    printf("spawn missing %d\n", err != 0);
    printf("waitpid nothing %d\n", await waitpid(-1, &status, 0));

    atexit(bye);
    atexit(bye2);
    assert(argc == 1);
    return 0;
}
