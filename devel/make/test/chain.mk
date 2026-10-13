%.o: %.c
	echo cc $< > $@
%.c: %.y
	echo yacc $< > $@
prog: prog.o
	echo link > $@
