SRCS := $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
all:
	echo [$(SRCS)] [$(wildcard nosuch.c a.c sub/*.x)] [$(wildcard *.nosuch)]
	echo [$(OBJS)] [$(wildcard sub/.h*)] [$(wildcard sub/*)]
