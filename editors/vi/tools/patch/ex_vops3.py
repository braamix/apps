p = 'ex_vops3.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# lf is a tag, not a function pointer. Upstream compared it against vmove and
# against lindent, which have nothing in common but a K&R `int (*)()`.
s = s.replace('int\t(*lf)();\n', '')
s = s.replace('\tlf = f;', '\tlf = (void *) f;')
s = s.replace('\t\tlf = lindent;', '\t\tlf = (void *) lindent;')
s = s.replace('if (lf == vmove && wcursor > linebuf)',
              'if (lf == (void *) op_move && wcursor > linebuf)')
s = s.replace("if (lf == lindent && linebuf[0] == '(')",
              "if (lf == (void *) lindent && linebuf[0] == '(')")

# The operator is compared against "just move" in three places.
s = s.replace('f != vmove', 'f != op_move')

s = s.replace('\tparens = any(*cp, "()") ? "()" : any(*cp, "[]") ? "[]" : "{}";',
              '\tparens = (char *)(any(*cp, "()") ? "()" : any(*cp, "[]") ? "[]" : "{}");')
s = s.replace('\treturn (ltosol1("()"));', '\treturn (ltosol1((char *) "()"));')

# showmatch. Upstream moved the cursor to the matching bracket, flushed, and
# slept a second so that you could see it. A sleep is a syscall and this is a
# plain function called from the middle of insert mode; and the frame is not
# sent until the next key is asked for, so the visit would never reach the
# screen anyway. The matching is still done, and an unmatched bracket still
# beeps -- what is lost is the pause.
s = s.replace('''		vgoto(splitw ? WECHO : LINE(wdot - llimit), column(wcursor) - 1);
		flush();
		sleep(1);
		vgoto(l, c);''', '''		vgoto(splitw ? WECHO : LINE(wdot - llimit), column(wcursor) - 1);
		flush();
		vgoto(l, c);''')
open(p, 'w').write(s)
