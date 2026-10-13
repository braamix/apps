.SUFFIXES: .c .o
.c.o:
	echo cc -c $<
VPATH = src
prog: main.o
	echo link $^
