p = 'ex_vget.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# getbr(): the one place visual mode reads, and so the one place it can send a
# frame. Upstream read a byte and coped with interrupted reads, Beehives and
# upper-case-only terminals; a key arrives whole here, so what is left is the
# shim in ex_screen.cpp plus two things upstream had no way to meet -- a
# resize, and the blit.
old = s[s.index('\tflusho();\nagain:'):s.index('\tlastvgk = 0;\n\tco_return (c);\n}')]
new = '''	/* The frame: the editor stops here, so the screen must be right here. */
	co_await vflush();

again:
	{
		Result<Key> r = Err(Error::NoMemory);

		if (Task<Result<Key>> t = vscreen->next_key())
			r = co_await t;
		if (r.is_err()) {
			/* Intr is a resize, or ^C, which answers ATTN. */
			if (r.error() == Error::Intr) {
				if (sig_take(SIG_INT))
					co_return (ATTN);
				vresize();
				co_await vflush();
				goto again;
			}
			if (r.error() == Error::Again)
				goto again;
			COTHROWV(0, error("Input read error"));
		}
		c = key_byte(res_of(r));
		if (c == 0)
			goto again;
	}

'''
s = s.replace(old, new)

# The Beehive arm, which turned f1 into an escape on a terminal that had no
# escape key. XB is false; the block is inside its own #ifdef and the
# configuration pass has already taken it, but Peek2key's declaration is not.
s = s.replace('\tstatic char Peek2key;\n', '')

# readecho saves and restores Pline, which is a typed pointer now.
s = s.replace('\tint (*OP)();', '\tPlineFn OP;')

# --- fastpeekkey(): the one-second window that told an arrow key from an
# escape typed by hand. There are no escape sequences: a named key arrives
# whole, so there is nothing to disambiguate and nothing to wait for. That is
# also the only place the editor read the clock.
old = s[s.index('\tif (value(TIMEOUT) && inopen >= 0) {'):
        s.index('trapalarm() {')]
s = s.replace(old, '''	/*
	 * Upstream waited a second to tell a three-byte arrow key from an ESC
	 * typed by hand. KEY_UP arrives as itself, so there is nothing to tell
	 * apart and `set notimeout` names a distinction that is gone.
	 */
	c = co_await peekkey();
	co_return (c);
}

''')
i = s.index('trapalarm() {')
j = s.index('\n}\n', i) + 3
s = s[:i] + s[j:]
open(p, 'w').write(s)
