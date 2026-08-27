p = 'ex_vadj.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# costSR and costAL counted the characters a reverse scroll and an add-line
# would take at the line's speed, so that vinslin could pick the cheaper. Both
# are one blit of the damage now, and the reverse-scroll arm is the one that
# keeps the screen image tidy at the top of the window.
s = s.replace('	} else if (SR && p == WTOP && costSR < costAL) {',
              '	} else if (SR && p == WTOP) {')

# The holdupd coalescing. Upstream deferred a redraw while more input was
# pending, by peeking at the keyboard here. That would make vrepaint() a
# coroutine, and vrepaint is called from twenty places in vmain -- it would
# take vsync, vscrap, vredraw and vcloseup with it. What it bought is what
# flush() already does: the damage rectangle is the union of what changed, so
# a repaint nobody sees costs nothing to send.
s = s.replace('''		if (holdupd)
			if (state == VISUAL)
				ignore(peekkey());
			else
				vup1();
		holdupd = 0;''', '''		if (holdupd && state != VISUAL)
			vup1();
		holdupd = 0;''')

# One of the nine CATCH blocks. This is the degenerate kind -- print the
# message, do not unwind -- so it is the message and a clearing of the flag.
s = s.replace('''		if (noteit(1) == 0 && odol == zero) {
			CATCH
				THROW(error("No lines in buffer"));
			ENDCATCH''', '''		if (noteit(1) == 0 && odol == zero) {
			error((char *) "No lines in buffer");
			(void) excatch();''')
open(p, 'w').write(s)
