CFLAGS = -O
all: prog other.o
prog: CFLAGS += -g
prog: main.o util.o
	echo link $@ [$(CFLAGS)]
util.o: override CFLAGS = -util
%.o: X := pat $(CFLAGS)
%.o:
	echo cc $(CFLAGS) $@ [$(X)]
other.o: export E = exported
other.o:
	echo cc $(CFLAGS) $@ "[$$E]"
