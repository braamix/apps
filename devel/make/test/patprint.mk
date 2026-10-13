%.o: %.c | hdr.h
	cc -c $<
all:
