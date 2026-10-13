PROGS = one two
define PROG_template
$(1): $(1).o
	echo link $$@ from $$^
ALL += $(1)
endef
all: progs late
$(foreach p,$(PROGS),$(eval $(call PROG_template,$(p))))
progs: $(ALL)
	echo [$(ALL)]
%.o:
	echo cc $@
$(eval LATER = set by eval)
late:
	echo [$(LATER)] $(eval RUNTIME = at run time)
	echo [$(RUNTIME)]
