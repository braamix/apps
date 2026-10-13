X = 1
ifeq ($(X),1)
$(error X is $(X))
endif
all:
	echo no
