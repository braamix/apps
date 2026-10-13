VPATH = other
prog: lib.a
	echo link $^
lib.a: util.c
	echo ar $@ $<
