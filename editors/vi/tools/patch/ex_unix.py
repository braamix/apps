p = 'ex_unix.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"\n#include "ex_screen.h"\n#include "ex_vis.h"\n\n#include "kernel/fmt.h"', 1)

# Everything from unixex to the end of the file is process handling, and all of
# it changes: there is no fork, no dup onto descriptor 0 or 1, and no second
# copy of the editor to feed a filter with. What replaces it is spawn(), and a
# temp file on each side of the pipeline instead of a pipe.
old = s[s.index('/*\n * Do the real work for execution of a shell escape.'):]
new = '''/*
 * Run a shell escape.
 *
 * Upstream forked, rearranged the child's descriptors, and exec'd the shell;
 * mode said whether a pipe was wanted on either side. Here spawn() takes the
 * descriptors as an argument, so the rearranging is the ChildIo, and the two
 * halves that had to be a fork -- the filter's input, which upstream wrote
 * from a *second copy of the editor* -- are temp files instead. One task
 * cannot park on two descriptors, so a filter driven through two pipes would
 * deadlock the moment either one filled.
 *
 * The claims have to go back before anything is spawned, and in this order:
 * the screen, then the keyboard, then the child, then the console. A
 * full-screen program claims the keyboard in its very first step, so a child
 * that races us for it loses (sh/job.cpp says so).
 */
static char in_name[32];
static char out_name[32];

static void tmpname(char *buf, char which)
{
	Buf<32> b;

	b.put("/tmp/vi").put(which).put('.').put(proc_pid());
	memcpy(buf, b.str().data(), b.str().size());
	buf[b.str().size()] = 0;
}

/*
 * The shell, and what it is told to run. Answers its exit status, or -1.
 */
static Task<int> runsh(char *opt, char *up, int fdin, int fdout)
{
	Str words[3];
	Args v;
	ChildIo cio;
	Result<u32> pid_r = Err(Error::NoMemory);
	u32 child;

	words[0] = Str(svalue(SHELL), strlen(svalue(SHELL)));
	words[1] = Str(opt, strlen(opt));
	words[2] = Str(up, strlen(up));
	v.v = Span<const Str>(words, 3);
	cio.in = fdin >= 0 ? (u32) fdin : SYS_STDIN;
	cio.out = fdout >= 0 ? (u32) fdout : SYS_STDOUT;

	if (Task<Result<u32>> t = spawn(v, cio))
		pid_r = co_await t;
	if (pid_r.is_err()) {
		errno = int(pid_r.error());
		co_return (-1);
	}
	child = res_of(pid_r);
	pid = (int) child;

	/*
	 * The console goes to the child so that ^C reaches it rather than the
	 * editor; a program that has not claimed the keyboard can ask for this
	 * only while it is itself in front.
	 */
	if (Task<Result<void>> t = set_fg(child))
		co_await t;
	{
		Result<Exited> w = Err(Error::NoMemory);

		if (Task<Result<Exited>> t = wait_child(child))
			w = co_await t;
		if (Task<Result<void>> t = set_fg(0))
			co_await t;
		if (w.is_err()) {
			errno = int(w.error());
			co_return (-1);
		}
		status = res_of(w).status;
		co_return (status);
	}
}

Task<void> unixex(char *opt, char *up, int newstdin, int mode)
{

	(void) mode;
	co_await runsh(opt, up, newstdin, -1);
	if (newstdin > 0)
		co_await ex_close(newstdin);
}

/*
 * Wait for the command to complete.
 * C flags suppression of printing.
 *
 * The waiting is done in runsh, which has to do it anyway to give the console
 * back; what is left of this is the "!" and the redraw.
 */
Task<void> unixwt(exbool c, int p)
{

	(void) p;
	if (!inopen && c && hush == 0) {
		printf("!\\n");
		flush();
		co_await exflush();
	}
}

/*
 * Set up the filtration implied by mode, which is like an open number: 1 means
 * the command's output replaces the range, 2 means the range is its input, 3
 * means both.
 *
 * Upstream built a pipe each way and forked a second editor to write the
 * range down the first one. Both ends of a pipeline cannot be driven from one
 * task here, so each side is a file in /tmp, which also makes the order
 * obvious: write the range, run the command, read the result back.
 */
Task<void> filter(int mode)
{
	int lines = lineDOL();
	int fdin = -1, fdout = -1;
	int saveio = io;

	mode++;
	tmpname(in_name, 'i');
	tmpname(out_name, 'o');

	if (mode & 2) {
		io = co_await ex_creat(in_name);
		if (io < 0)
			COTHROW(filioerr(in_name));
		co_await putfile();
		COCHK;
		co_await ex_close(io);
		io = -1;
		fdin = co_await ex_open(in_name, 0);
		if (fdin < 0)
			COTHROW(filioerr(in_name));
	}
	if (mode & 1) {
		fdout = co_await ex_creat(out_name);
		if (fdout < 0) {
			if (fdin >= 0)
				co_await ex_close(fdin);
			COTHROW(filioerr(out_name));
		}
	}

	co_await vspawn_begin();
	co_await runsh((char *) "-c", uxb, fdin, fdout);
	co_await vspawn_end();

	if (fdout >= 0)
		co_await ex_close(fdout);
	if (mode == 3) {
		exdelete(0);
		addr2 = addr1 - 1;
	}
	if (mode & 1) {
		if (FIXUNDO)
			undap1 = undap2 = addr2+1;
		io = co_await ex_open(out_name, 0);
		if (io < 0)
			COTHROW(filioerr(out_name));
		ignore(co_await append(getfile, addr2));
		co_await ex_close(io);
	}
	io = saveio;
	{
		Str n = Str(in_name, strlen(in_name));
		Str o = Str(out_name, strlen(out_name));

		if (mode & 2)
			if (Task<Result<void>> t = remove_path(n, false))
				co_await t;
		if (mode & 1)
			if (Task<Result<void>> t = remove_path(o, false))
				co_await t;
	}
	co_await unixwt(!inopen, 0);
	netchHAD(lines);
}
'''
s = s.replace(old, new)

s = s.replace('''		case '\\\\':''', '''		case '\\\\':''')
open(p, 'w').write(s)
print('ok')
