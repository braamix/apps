WHO := $(shell echo braam)
X = one
LATER = $(shell echo $(X) two)
ifeq ($(shell echo y),y)
COND = taken
endif
TARGETS = $(shell echo t1 t2)
define D :=
$(shell echo in define)
endef
export E = $(shell echo exported)

all: $(TARGETS)
	echo [$(WHO)] [$(LATER)] [$(COND)] [$(D)] "[$$E]"
	echo [$(shell echo a; echo b)] [$(shell exit 3)]
	echo hi > made
	echo [$(wildcard made)]
$(TARGETS):
	echo $@
