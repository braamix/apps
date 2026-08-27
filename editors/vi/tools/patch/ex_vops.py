p = 'ex_vops.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

s = s.replace('\tint (*OO)();', '\tOutcharFn OO;')
s = s.replace('\tint copyw(), copywR();\n', '')

# vremote applies one of eight functions to a range; three of them await, so
# the type is the awaiting one and the rest are reached through the wrappers
# in ex_vmain.cpp.
for a, b in (('vremote(i, exdelete, 0)', 'vremote(i, op_delete, 0)'),
             ('vremote(cnt, exdelete, 0)', 'vremote(cnt, op_delete, 0)'),
             ('vremote(cnt, vshift, 0)', 'vremote(cnt, op_shift, 0)'),
             ('vremote(cnt, filter, 2)', 'vremote(cnt, op_filter, 2)'),
             ('vremote(cnt, YANKreg, vreg)', 'vremote(cnt, op_yankreg, vreg)'),
             ("vremote(cnt, YANKreg, '1')", "vremote(cnt, op_yankreg, '1')"),
             ('vremote(cnt, yank, 0)', 'vremote(cnt, op_yank, 0)'),
             ('vremote(1, yank, 0)', 'vremote(1, op_yank, 0)')):
    s = s.replace(a, b)
open(p, 'w').write(s)

p = 'ex_vops.cpp'
s = open(p).read()

# Two of the nine CATCH blocks, both in vfilter: one around parsing the
# command, one around running it. Each guarded body is a co_await, so it stays
# where it is -- vcatch says an error comes back here, and excatch() is the
# landing.
s = s.replace('''	CATCH
		fixech();
		co_await unix0(0);
	ONERR
		splitw = 0;
		ungetchar(d);
		vrepaint(cursor);
		globp = oglobp;
		co_return;
	ENDCATCH''', '''	vcatch = 1;
	fixech();
	co_await unix0(0);
	vcatch = 0;
	if (excatch()) {
		splitw = 0;
		ungetchar(d);
		vrepaint(cursor);
		globp = oglobp;
		co_return;
	}''')

s = s.replace('''	CATCH
		vgoto(WECHO, 0); flusho();
		co_await vremote(cnt, op_filter, 2);
	ONERR
		vdirty(0, LINES);
	ENDCATCH''', '''	vcatch = 1;
	vgoto(WECHO, 0); flusho();
	co_await vremote(cnt, op_filter, 2);
	vcatch = 0;
	if (excatch())
		vdirty(0, LINES);''')
open(p, 'w').write(s)
