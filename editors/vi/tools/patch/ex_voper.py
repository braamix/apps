import re

p = 'ex_voper.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# The operator table. Upstream held any of seven functions in an `int (*)()`
# and called it with whatever suited; three of the seven await now, so the type
# is the awaiting one and the other four are reached through the wrappers in
# ex_vmain.cpp.
s = s.replace('\tint (*moveop)(), (*deleteop)();', '\tVopf moveop, deleteop;')
s = s.replace('\tint (*opf)();', '\tVopf opf;')
s = s.replace('moveop = vmove, deleteop = vdelete;',
              'moveop = op_move, deleteop = vdelete;')
s = s.replace('moveop = vdelete;', 'moveop = vdelete;')
s = s.replace('moveop = vyankit;', 'moveop = vyankit;')
s = s.replace('moveop = vshftop;', 'moveop = vshftop;')
s = s.replace('deleteop = obeep;', 'deleteop = op_beep;')

# HOLDWIG stopped the screen wiggling as a bracket motion redrew it, and was
# worth doing above 300 baud. Everything is above 300 baud.
s = s.replace('\t\tif (ospeed > B300)\n\t\t\thold |= HOLDWIG;',
              '\t\thold |= HOLDWIG;')
s = re.sub(r'\bopf = moveop;', 'opf = moveop;', s)

# Every comparison against "is the operator a plain move".
s = s.replace('opf != vmove', 'opf != op_move')
s = s.replace('opf == vmove', 'opf == op_move')

# And the one place it is applied.
s = s.replace('\t(*opf)(c);', '\tco_await (*opf)(c);')

# word() and eend() only ever compare what they are handed.
s = s.replace('\tint (*op)();\n', '')
s = s.replace('op != vmove', 'op != op_move')
s = s.replace('op == vmove', 'op == op_move')
open(p, 'w').write(s)

p = 'ex_voper.cpp'
s = open(p).read()

s = s.replace("\t\tvglobp = vscandir[0] == '/' ? \"?\" : \"/\";",
              "\t\tvglobp = (char *)(vscandir[0] == '/' ? \"?\" : \"/\");")

# One of the nine CATCH blocks. The guarded statement is a plain call, so
# nothing has to be lifted: vcatch says "an error here comes back to me",
# address() records rather than throwing, and excatch() is the landing.
s = s.replace('''		CATCH
			addr = address(cursor);
		ONERR
slerr:''', '''		vcatch = 1;
		addr = address(cursor);
		vcatch = 0;
		if (excatch()) {
slerr:''')
s = s.replace('''			vjumpto(dot, ocurs, 0);
			co_return;
		ENDCATCH''', '''			vjumpto(dot, ocurs, 0);
			co_return;
		}''')
s = s.replace('\t\tif (globp == 0)\n\t\t\tglobp = "";',
              '\t\tif (globp == 0)\n\t\t\tglobp = (char *) "";')
open(p, 'w').write(s)
