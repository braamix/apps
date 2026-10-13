include inc1.mk
-include nosuch.mk
sinclude nosuch2.mk
CFG = base
include $(INCDIR)/inc2.mk
all: $(OBJ)
	echo [$(CFG)] [$(FROM1)] [$(FROM2)] [$(OBJ)]
