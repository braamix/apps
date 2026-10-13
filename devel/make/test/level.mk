all:
	@echo [$(MAKELEVEL)]
	@$(MAKE) -s -f level.mk sub
sub:
	@echo [$(MAKELEVEL)]
