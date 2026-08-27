p = 'ex_vwind.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# vcookit asked whether a scroll was worth doing gently or whether the screen
# should just be redrawn, and answered from the line's speed. There is no line
# and no speed: a repaint costs one blit of what actually changed, so the
# question is only about how far the scroll goes.
s = s.replace('	return (cnt > 1 && (ospeed < B1200 && !initev || cnt > LINES * 2));',
              '	return (cnt > 1 && cnt > LINES * 2);')
open(p, 'w').write(s)
