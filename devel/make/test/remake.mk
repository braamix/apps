include gen.mk
all:
	echo [$(GEN)] [$(MAKE_RESTARTS)]
gen.mk: src
	echo "GEN = made" > $@
