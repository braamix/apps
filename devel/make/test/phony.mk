.PHONY: all clean
all: prog
	echo all done
prog: a.c b.c
	echo build $@ from $^ first $<
clean:
	echo clean
