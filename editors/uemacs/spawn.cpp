/*	spaw.c
 *
 *	Various operating system access commands.
 *
 *	<odified by Petri Kutvonen
 */

#include "estruct.h"
#include "globals.h"
#include "efunc.h"

#include "proc/io.h"

/* unlink(), which is a syscall here and so is awaited. */
static Task<void> unlink(const char *path)
{
    if (Task<Result<void>> t = remove_path(Str(path, strlen(path)), false))
        co_await t;
}

/*
 * system(), which is what every command below was written against.  Upstream
 * forked a shell and waited; spawn() takes the descriptors as an argument, so
 * the fork is gone and what is left is the shell, the wait, and handing the
 * console over so that ^C reaches the child rather than the editor.
 *
 * The screen and the keyboard are the callers' business, as they always were:
 * each one calls tcapclose() before this and tcapopen() after.
 */
static Task<int> system(const char *cmd)
{
    Str words[3];
    Args v;
    Result<u32> pid_r = Err(Error::NoMemory);
    Result<Exited> w  = Err(Error::NoMemory);
    char *sh          = getenv("SHELL");
    u32 child;

    if (sh == NULL || *sh == 0)
        sh = (char *)"/bin/sh";
    words[0] = Str(sh, strlen(sh));
    words[1] = Str("-c", 2);
    words[2] = Str(cmd, strlen(cmd));
    v.v      = Span<const Str>(words, 3);

    if (Task<Result<u32>> t = spawn(v))
        pid_r = co_await t;
    if (pid_r.is_err())
        co_return -1;
    child = pid_r.value();

    if (Task<Result<void>> t = set_fg(child))
        co_await t;
    if (Task<Result<Exited>> t = wait_child(child))
        w = co_await t;
    if (Task<Result<void>> t = set_fg(0))
        co_await t;
    if (w.is_err())
        co_return -1;
    co_return w.value().status;
}

/*
 * Remember what a subprocess exited with, for $rval.  wait_child() answers
 * the status the shell would report as $?, so unlike upstream's wait status
 * there is nothing to take apart.
 */
static void record_status(int status)
{
    subprocess_status = status;
}

/*
 * Create a subjob with a copy of the command intrepreter in it. When the
 * command interpreter exits, mark the screen as garbage so that you do a full
 * repaint. Bound to "^X C". The message at the start in VMS puts out a newline.
 * Under some (unknown) condition, you don't get one free when DCL starts up.
 */
Task<int> cmd_interactive_shell(int f, int n)
{
    char *cp;

    /* don't allow this command if restricted */
    if (restflag)
        co_return restricted_error();

    movecursor(term.t_nrow, 0); /* Seek to last line.   */
    ttflush();
    co_await tcapclose(); /* stty to old settings */
    tcapkclose();         /* Close "keyboard" */
    if ((cp = getenv("SHELL")) != NULL && *cp != '\0')
        record_status(co_await system(cp));
    else
        record_status(co_await system("exec /bin/sh"));
    screen_garbage = TRUE;
    co_await tcapopen();
    tcapkopen();
    co_return TRUE;
}

/*
 * Suspend, which there is no way to do: SIG_TSTP is not in Braam's catchable
 * set and nothing stops a process and gives the shell its prompt back.  Say
 * so rather than pretending, the way "C-x C" and "C-x !" still work.
 */
Task<int> cmd_suspend_emacs(int f, int n)
{
    msg_printf("(No job control)");
    co_return FALSE;
}

Task<void> rtfrmshell(void)
{
    co_await tcapopen();
    curwp->w_flag  = WFHARD;
    screen_garbage = TRUE;
}

/*
 * Run a one-liner in a subjob. When the command returns, wait for a single
 * character to be typed, then mark the screen as garbage so a full repaint is
 * done. Bound to "C-X !".
 */
Task<int> cmd_shell_command(int f, int n)
{
    int s;
    char line[NLINE];

    /* don't allow this command if restricted */
    if (restflag)
        co_return restricted_error();

    if ((s = co_await ask_string("!", line, NLINE)) != TRUE)
        co_return s;
    ttflush();
    co_await tcapclose(); /* stty to old modes    */
    tcapkclose();
    record_status(co_await system(line));

    /* Before the screen is taken back, because taking it is what paints
       over the output the pause exists to let you read. */
    if (executing_command_line == FALSE)
        co_await tcappause("(End)");
    co_await tcapopen();
    tcapkopen();
    screen_garbage = TRUE;
    co_return TRUE;
}

/*
 * Run an external program with arguments. When it returns, wait for a single
 * character to be typed, then mark the screen as garbage so a full repaint is
 * done. Bound to "C-X $".
 */

Task<int> cmd_execute_program(int f, int n)
{
    int s;
    char line[NLINE];

    /* don't allow this command if restricted */
    if (restflag)
        co_return restricted_error();

    if ((s = co_await ask_string("!", line, NLINE)) != TRUE)
        co_return s;
    ttputc('\n'); /* Already have '\r'    */
    ttflush();
    co_await tcapclose(); /* stty to old modes    */
    tcapkclose();
    record_status(co_await system(line));
    co_await tcappause("(End)"); /* Pause, before retaking. */
    co_await tcapopen();
    screen_garbage = TRUE;
    co_return TRUE;
}

/*
 * filter a buffer through an external DOS program
 * Bound to ^X #
 */
/*
 * Run a command and read what it printed into a window of its own, in
 * view mode.  Bound to "C-x @".
 *
 * The output goes through a file called "command" in the current
 * directory, which is how filter-buffer's fltinp and fltout work two
 * functions down; all three share the same weakness about where they put
 * it and what happens if something is there already.
 */
Task<int> cmd_pipe_command(int f, int n)
{
    struct window *wp;
    struct buffer *bp;
    char line[NLINE];
    int s;
    static char bname[]  = "command";
    static char filnam[] = "command";

    /* don't allow this command if restricted */
    if (restflag)
        co_return restricted_error();

    if ((s = co_await ask_string("@", line, NLINE)) != TRUE)
        co_return s;

    /* if the last one is still around, get it off the screen and go */
    bp = find_buffer(bname, FALSE, 0);
    if (bp != NULL) {
        for (wp = window_head; wp != NULL; wp = wp->w_wndp) {
            if (wp->w_bufp != bp)
                continue;
            if (wp == curwp)
                co_await cmd_delete_window(FALSE, 1);
            else
                co_await cmd_delete_other_windows(FALSE, 1);
            break;
        }
        if (co_await destroy_buffer(bp) != TRUE)
            co_return FALSE;
    }

    ttflush();
    co_await tcapclose(); /* stty to old modes    */
    tcapkclose();
    /*
     * The space before the '>' matters: without it a command ending in
     * a digit has that digit read as a file descriptor number, so "seq
     * 4" becomes "seq" with fd 4 redirected and nothing comes back.
     * filter-buffer below has always had the space.
     */
    strcat(line, " >");
    strcat(line, filnam);
    record_status(co_await system(line));
    co_await tcapopen();
    tcapkopen();
    ttflush();
    screen_garbage = TRUE;

    if (co_await cmd_split_current_window(FALSE, 1) == FALSE)
        co_return FALSE;
    if (co_await getfile(filnam, FALSE) == FALSE)
        co_return FALSE;

    curwp->w_bufp->b_mode |= MDVIEW;
    update_modeline();
    co_await unlink(filnam);
    co_return TRUE;
}

Task<int> cmd_filter_buffer(int f, int n)
{
    int s;                     /* return status from CLI */
    struct buffer *bp;         /* pointer to buffer to zot */
    char line[NLINE];          /* command line send to shell */
    char tmpnam[NFILEN];       /* place to store real file name */
    struct filestate tmpstate; /* and the state that goes with it */
    static char bname1[] = "fltinp";

    static char filnam1[] = "fltinp";
    static char filnam2[] = "fltout";

    /* don't allow this command if restricted */
    if (restflag)
        co_return restricted_error();

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if      */
        co_return readonly_error(); /* we are in read only mode     */

    /* get the filter name and its args */
    if ((s = co_await ask_string("#", line, NLINE)) != TRUE)
        co_return s;

    /* setup the proper file names */
    bp = curbp;
    strcpy(tmpnam, bp->b_fname); /* save the original name */
    tmpstate = bp->b_fstate;     /* and what we know about it */
    strcpy(bp->b_fname, bname1); /* set it to our new one */

    /* write it out, checking for errors */
    if (co_await writeout(filnam1) != TRUE) {
        msg_printf("(Cannot write filter file)");
        strcpy(bp->b_fname, tmpnam);
        bp->b_fstate = tmpstate;
        co_return FALSE;
    }
    ttputc('\n'); /* Already have '\r'    */
    ttflush();
    co_await tcapclose(); /* stty to old modes    */
    tcapkclose();
    strcat(line, " <fltinp >fltout");
    record_status(co_await system(line));
    co_await tcapopen();
    tcapkopen();
    ttflush();
    screen_garbage = TRUE;
    s              = TRUE;

    /* on failure, escape gracefully */
    if (s != TRUE || (co_await readin(filnam2, FALSE) == FALSE)) {
        msg_printf("(Execution failed)");
        strcpy(bp->b_fname, tmpnam);
        bp->b_fstate = tmpstate;
        co_await unlink(filnam1);
        co_await unlink(filnam2);
        co_return s;
    }

    /*
     * Reset file name.  The state goes back with it: readin() has just
     * recorded what the filter's output file looked like, which says
     * nothing at all about the file the buffer is really for - and we
     * have not touched that one, so what we knew about it still holds.
     */
    strcpy(bp->b_fname, tmpnam); /* restore name */
    bp->b_fstate = tmpstate;
    bp->b_flag |= BFCHG; /* flag it as changed */

    /* and get rid of the temporary file */
    co_await unlink(filnam1);
    co_await unlink(filnam2);
    co_return TRUE;
}
