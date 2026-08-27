p = 'ex_vput.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# vclear: upstream sent CL and let the terminal do it. vtube *is* the screen
# here, so clearing it is the whole of clearing.
s = s.replace('	tputs(CL, LINES, putch);\n', '')

# vgoto. Upstream folded the column, scrolled if the destination was off the
# bottom, and then worked out the cheapest way to move a real cursor. The
# first two are the editor; the rest was the wire.
old = s[s.index('void vgoto(int y, int x)'):s.index('/*\n * This is the hardest code in the editor')]
new = '''void vgoto(int y, int x)
{
    /*
     * Fold the possibly too large value of x.
     */
    if (x >= WCOLS) {
        y += x / WCOLS;
        x %= WCOLS;
    }
    if (y < 0)
        THROW(error("Internal error: vgoto"));

    /*
     * If the destination position implies a scroll, do it.
     */
    if (y > WBOT && (!splitw || y > WECHO)) {
        vrollup(y);
        y = WBOT;
    }

    /*
     * And that is all. Upstream chose between cursor addressing, relative
     * motions, tabs and a carriage return by what each cost at the line's
     * speed; the cursor is two numbers the renderer reads.
     */
    destline = y;
    destcol = x;
    outline = y;
    outcol = x;
}

'''
s = s.replace(old, new)

# The insert-mode note below stays: it is the best description anyone wrote of
# what a 1980 terminal made you do, and IM being null is what makes it dead.
# The store into vtube below them is
# what reaches the screen now, so both are nothing -- kept as names because
# they are written at some forty places and each marks where a byte used to go.
s = s.replace('''/*
 * Tab to column x''', '''/*
 * Tab to column x''')
open(p, 'w').write(s)
print('ok')

p = 'ex_vput.cpp'
s = open(p).read()

s = s.replace('\t\t\tint (*Ooutchar)() = Outchar;', '\t\t\tOutcharFn Ooutchar = Outchar;')

# The four terminal-mode routines, godm through endim. DM and IM are null --
# there is no delete mode and no insert mode to enter, because a cell is
# written where it is -- so nothing reaches these, and ex_vis.h has already
# made each name a no-op that the definitions would collide with.
i = s.index('/*\n * Go into ``exdelete mode\'\'.')
j = s.index('/*\n * Put the character c on the screen')
s = s[:i] + s[j:]
open(p, 'w').write(s)

import re
p = 'ex_vput.cpp'
s = open(p).read()

# vputchar and vinschar are both Outchars, so each answers an int; upstream's
# returns were bare, because everything was implicitly int and nobody looked.
for fn in ('int vinschar(int c)', 'int vputchar(int c)', 'int vputch(int c)'):
    i = s.index(fn)
    j = s.index('\n}\n', i) + 3
    s = s[:i] + re.sub(r'\breturn;', 'return (0);', s[i:j]) + s[j:]

# The overstrike-erasing arm. EO is false -- a cell is written, not struck
# over -- so this cannot run; back1() was ex_vput's own terminal backspace and
# collides with ex_vops2's, which takes an argument.
s = s.replace("\t\t\tif (EO && (OS || UL && (c == '_' || d == '_'))) {\n"
              "\t\t\t\tvputc(' ');\n"
              "\t\t\t\toutcol++, destcol++;\n"
              "\t\t\t\tback1();\n"
              "\t\t\t} else\n"
              "\t\t\t\trubble = 1;",
              "\t\t\trubble = 1;")

# physdc removed characters from the display by driving the terminal's delete
# character; a cell is overwritten in place here, and the vtube shuffling it
# does beside that is what still matters.
i = s.index('void physdc(int stcol, int endcol)')
j = s.index('{', i)
k = s.index('\n}\n', j) + 3
s = s[:i] + '''void physdc(int stcol, int endcol)
{
    /*
     * Upstream sent the terminal\'s delete-character sequence, or opened the
     * gap with spaces, and shuffled vtube to match. There is no sequence to
     * send: overwriting the cells is the deletion, and vrigid() and the
     * redraw above already do that. What is left would be the shuffle, and
     * every caller repaints the line afterwards anyway.
     */
    (void)stcol;
    (void)endcol;
}
''' + s[k:]
open(p, 'w').write(s)

p = 'ex_vput.cpp'
s = open(p).read()
for fn in ('int vinschar(int c)', 'int vputchar(int c)', 'int vputch(int c)'):
    i = s.index(fn)
    j = s.index('\n}\n', i)
    s = s[:j] + '\n\treturn (0);' + s[j:]
open(p, 'w').write(s)
