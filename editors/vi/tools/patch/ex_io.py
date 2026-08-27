p = 'ex_io.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

s = s.replace('exbool filename(int comm)', 'char *filename(int comm)')

# --- glob(): upstream forked a shell to expand the argument words, because
# the v7 shell was the only thing that knew how. The shell here globs before
# ex is entered, so what arrives is already expanded and this only has to
# split it; gscan() is what decided, and it decides the same way every time.
old = s[s.index('void glob(struct glob *gp)'):s.index('/*\n * Scan genbuf for shell metacharacters.')]
new = '''/*
 * Split the argument words in genbuf.
 *
 * Upstream globbed them, by writing "echo <words>" down a pipe to a forked
 * shell and reading the expansion back -- the shell was the only thing that
 * knew what * and ? and ~ meant. Here the shell has already done that before
 * ex was entered, so a name that reaches this has been expanded already, and
 * what is left is the splitting. See the note in the README: :e *.c is the
 * shell's business, not the editor's.
 */
void glob(struct glob *gp)
{
	char **argv = gp->argv;
	char *cp = gp->argspac;
	char *v = genbuf + 5;		/* strlen("echo ") */

	gp->argc0 = 0;
	for (;;) {
		while (isspace(*v))
			v++;
		if (!*v)
			break;
		*argv++ = cp;
		while (*v && !isspace(*v))
			*cp++ = *v++;
		*cp++ = 0;
		gp->argc0++;
		if (gp->argc0 >= NARGS)
			THROW(error("Arg list too long"));
	}
	*argv = 0;
}

'''
s = s.replace(old, new)

# --- rop(): open, size, and the shape of what was opened. Upstream also
# refused block and character specials and looked for an a.out magic number in
# the first two bytes; there are no special files, and a wasm binary is not
# one of the seven PDP-11 magics, so what is left is the directory.
old = s[s.index('Task<void> rop(int c)'):s.index('Task<void> rop2(void)')]
new = '''Task<void> rop(int c)
{
	static int ovro;	/* old value(READONLY) */
	static int denied;	/* 1 if READONLY was set due to file permissions */
	struct exstat stbuf;

	io = co_await ex_open(file, 0);
	if (io < 0) {
		if (c == 'e' && errno == int(Error::NotFound)) {
			edited++;
			/*
			 * If the user just did "ex foo" he is probably
			 * creating a new file.  Don't be an error, since
			 * this is ugly, and it screws up the + option.
			 */
			if (!seenprompt) {
				printf(" [New file]");
				noonl();
				co_return;
			}
		}
		COTHROW(syserror());
	}
	if (co_await ex_fstat(io, &stbuf))
		COTHROW(syserror());
	if (stbuf.st_isdir)
		COTHROW(error(" Directory"));
	if (c != 'r') {
		if (value(READONLY) && denied) {
			value(READONLY) = ovro;
			denied = 0;
		}
	}
	if (value(READONLY)) {
		printf(" [Read only]");
		flush();
	}
	if (c == 'r')
		setdot();
	else
		setall();
	if (FIXUNDO && inopen && c == 'r')
		undap1 = undap2 = dot + 1;
	co_await rop2();
	COCHK;
	co_await rop3(c);
}

'''
s = s.replace(old, new)
open(p, 'w').write(s)
print('ok')

p = 'ex_io.cpp'
s = open(p).read()

s = s.replace('exbool getargs(void)\n', 'int getargs(void)\n')

s = s.replace('	if (iostats() == 0 && c == \'e\')', '	if (co_await iostats() == 0 && c == \'e\')')
s = s.replace('	globp = (*firstpat) ? firstpat : "$";',
              '	globp = (*firstpat) ? firstpat : (char *) "$";')

# samei() asked whether two names were the same inode, so that :w would refuse
# to write over a different file that happened to share a name, and so that a
# character special could be recognised as /dev/null. The store has no inode
# number and no device, and there are no special files; the names are what
# there is to compare.
s = s.replace('''void samei(struct exstat *stb, char *cp)
{
	struct stat stb;

	if (stat(cp, &stb) < 0 || sp->st_dev != stb.st_dev)
		return (0);
	return (sp->st_ino == stb.st_ino);
}''', '''exbool samei(struct exstat *sp, char *cp)
{

	(void) sp;
	return (eq(file, cp));
}''')
open(p, 'w').write(s)


p = 'ex_io.cpp'
s = open(p).read()

# filename() answers the string it parsed; the last arm fell off the end.
s = s.replace('''	if (savedfile[0] == 0 && c != 'f')
		THROWV(0, error("No file|No current filename"));''',
'''	if (savedfile[0] == 0 && c != 'f')
		THROWV(0, error("No file|No current filename"));''')

# samei(): upstream compared inode and device, so that :w would refuse to
# overwrite a different file of the same name and so that /dev/null and
# /dev/tty could be recognised. The store keeps neither number, and neither
# special file is one you can write a buffer to; the names are the comparison.
s = s.replace('''exbool samei(struct exstat *sp, char *cp)
{
	struct stat stb;

	if (stat(cp, &stb) < 0 || sp->st_dev != stb.st_dev)
		return (0);
	return (sp->st_ino == stb.st_ino);
}''', '''exbool samei(struct exstat *sp, char *cp)
{

	(void) sp;
	return (eq(file, cp));
}''')

# --- wop(): the write. The clearance checks that survive are the ones about
# the buffer -- read only, a partial buffer, a file that already exists -- and
# not the ones about what kind of file it is, since every path names a file.
old = s[s.index('Task<void> wop(exbool dofname)'):s.index('/*\n * Is file the edited file?')]
new = '''Task<void> wop(exbool dofname)
{
	int c, exclam, nonexist;
	line *saddr1, *saddr2;
	struct exstat stbuf;

	c = 0;
	exclam = 0;
	if (dofname) {
		if (peekchar() == '!')
			exclam++, ignchar();
		ignore(skipwh());
		while (peekchar() == '>')
			ignchar(), c++, ignore(skipwh());
		if (c != 0 && c != 2)
			COTHROW(error("Write forms are 'w' and 'w>>'"));
		filename('w');
		COCHK;
	} else {
		if (savedfile[0] == 0)
			COTHROW(error("No file|No current filename"));
		saddr1=addr1;
		saddr2=addr2;
		addr1=one;
		addr2=dol;
		CP(file, savedfile);
		if (inopen) {
			vclrech(0);
			splitw++;
		}
		lprintf("\\"%s\\"", file);
	}
	nonexist = co_await ex_stat(file, &stbuf);
	switch (c) {

	case 0:
		if (!exclam && (!value(WRITEANY) || value(READONLY)))
		switch (edfile()) {

		case NOTEDF:
			if (nonexist)
				break;
			COTHROW(serror(" File exists| File exists - use \\"w! %s\\" to overwrite",
				       file));

		case EDF:
			if (value(READONLY))
				COTHROW(error(" File is read only"));
			break;

		case PARTBUF:
			if (value(READONLY))
				COTHROW(error(" File is read only"));
			COTHROW(error(" Use \\"w!\\" to write partial buffer"));
		}
cre:
		io = co_await ex_creat(file);
		if (io < 0)
			COTHROW(syserror());
		writing = 1;
		if (hush == 0)
			if (nonexist)
				printf(" [New file]");
			else if (value(WRITEANY) && edfile() != EDF)
				printf(" [Existing file]");
		break;

	case 2:
		io = co_await ex_open(file, 1);
		if (io < 0) {
			if (exclam || value(WRITEANY))
				goto cre;
			COTHROW(syserror());
		}
		co_await ex_seek(io, 0L, 2);
		break;
	}
	co_await putfile(0);
	COCHK;
	ignore(co_await iostats());
	if (c != 2 && addr1 == one && addr2 == dol) {
		if (eq(file, savedfile))
			edited = 1;
		sync();
	}
	if (!dofname) {
		addr1 = saddr1;
		addr2 = saddr2;
	}
	writing = 0;
}

'''
s = s.replace(old, new)
open(p, 'w').write(s)

p = 'ex_io.cpp'
s = open(p).read()

s = s.replace('co_await putfile(0);', 'co_await putfile();')

# ninbuf counted down the read buffer; it lived in ex_temp.h with the rest of
# the buffering, and the rest of the buffering went with the temp file.
s = s.replace('static\tchar *nextip;', 'static\tchar *nextip;\nstatic\tint ninbuf;')

s = s.replace('''			ninbuf = read(io, genbuf, LBSIZE) - 1;''',
              '''			ninbuf = co_await ex_read(io, genbuf, LBSIZE) - 1;''')

s = s.replace('''				if (write(io, genbuf, nib) != nib) {
					wrerror();
				}
				cntch += nib;''',
              '''				if (co_await ex_write(io, genbuf, nib) != nib) {
					wrerror();
					COCHK;
				}
				cntch += nib;''')
s = s.replace('''	if (write(io, genbuf, nib) != nib) {
		wrerror();
	}
	cntch += nib;''', '''	if (co_await ex_write(io, genbuf, nib) != nib) {
		wrerror();
		COCHK;
	}
	cntch += nib;''')
open(p, 'w').write(s)

p = 'ex_io.cpp'
s = open(p).read()

# --- source(): upstream closed file descriptor 0 and opened the script over
# it, so that getach() read from the script without knowing. There is no dup
# and no descriptor renumbering here, and no need for either: the script is
# read whole and handed to commands() as globp, which is what EXINIT already
# was. That also removes the "Too many nested sources" limit, which counted
# descriptors.
old = s[s.index('Task<void> source(char *fil, exbool okfail)'):
        s.index('/*\n * Clear io statistics before a read or write.')]
new = '''Task<void> source(char *fil, exbool okfail)
{
	int ointty;
	char savepeekc, *saveglobp;
	char *text;

	savepeekc = peekc;
	saveglobp = globp;
	peekc = 0; globp = 0;

	{
		Result<String> r = Err(Error::NoMemory);

		if (Task<Result<String>> t = read_file(Str(fil, strlen(fil))))
			r = co_await t;
		if (r.is_err()) {
			errno = int(r.error());
			peekc = savepeekc;
			globp = saveglobp;
			if (!okfail)
				COTHROW(filioerr(fil));
			co_return;
		}
		{
			Str got = res_of(r).str();

			text = (char *) heap_alloc(got.size() + 1);
			if (text == 0) {
				peekc = savepeekc;
				globp = saveglobp;
				COTHROW(error(" Out of memory"));
			}
			memcpy(text, got.data(), got.size());
			text[got.size()] = 0;
		}
	}

	slevel++;
	ointty = intty;
	intty = 0;
	oprompt = value(PROMPT);
	value(PROMPT) = 0;
	globp = text;
	co_await commands(1, 1);
	intty = ointty;
	value(PROMPT) = oprompt;
	globp = saveglobp;
	peekc = savepeekc;
	slevel--;
	heap_free(text);
}

'''
s = s.replace(old, new)
s = s.replace('#include "ex_buf.h"', '#include "ex_buf.h"\n\n#include "kernel/alloc.h"', 1)
open(p, 'w').write(s)

p = 'ex_io.cpp'
s = open(p).read()
s = s.replace('''Task<int> iostats(void)
{

	close(io);
	io = -1;''', '''Task<int> iostats(void)
{

	co_await ex_close(io);
	io = -1;''')
# %D was the v6 printf's long decimal; %ld is this one's.
s = s.replace('" %d/%D"', '" %d/%ld"')
s = s.replace('" %d line%s, %D character%s"', '" %d line%s, %ld character%s"')
s = s.replace('"%D null"', '"%ld null"')
s = s.replace('"%D non-ASCII"', '"%ld non-ASCII"')
open(p, 'w').write(s)

p = 'ex_io.cpp'
s = open(p).read()
# The samef label was the target of a goto in the arm that recognised the name
# as the file already open, and that arm went with the inode comparison.
s = s.replace('		THROW(error("Filename too long"));\nsamef:\n',
              '		THROW(error("Filename too long"));\n')
s = s.replace('	glob(&G);\n	if (G.argc0 > 1)', '	glob(&G);\n	CHK;\n	if (G.argc0 > 1)')
open(p, 'w').write(s)
