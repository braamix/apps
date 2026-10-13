A = x
E =
ifeq ($(A),x)
R1 = paren
endif
ifeq "$(A)" 'x'
R2 = quotes
endif
ifneq ($(A), y)
R3 = neq
else
R3 = wrong
endif
ifdef A
ifndef E
R4 = def
endif
endif
ifeq ($(A),y)
R5 = wrong
else ifeq ($(A),x)
R5 = elseif
else
R5 = wrong too
endif
ifeq (1,2)
  this line is no makefile at all: !
  ifeq (nested,skipped)
  endif
else
R6 = nested
endif
ifeq ( $(A),x)
R7 = wrong
else
R7 = blanks
endif

all:
ifdef A
	echo $(R1) $(R2) $(R3) $(R4) $(R5) $(R6) $(R7)
else
	echo wrong
endif
	echo after
