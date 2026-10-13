VPATH = src:other
vpath %.h inc
prog: main.o util.o
	echo link $^
%.o: %.c defs.h
	echo cc -c $< -o $@ [$^]
