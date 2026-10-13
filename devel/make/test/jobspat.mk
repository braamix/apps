prog: x.o y.o z.o
	echo link $^
%.o: %.c
	echo cc $<
