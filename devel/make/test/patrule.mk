%.o: %.c hdr.h
	echo cc -c $< -o $@ [$*] [$^]
prog: a.o b.o bb.o x.o sub/a.o sub/x.o
	echo link $^ [$+]
b%.o: b%.c
	echo special $@ [$*]
sub/%.o: %.c
	echo in sub $@ [$*] $<
