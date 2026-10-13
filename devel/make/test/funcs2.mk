X = $(Y) and more
Y = why
S := simple
reverse = $(2) $(1)
pair = [$(0):$(1),$(2)]
nested = $(call pair,$(1)x,$(call reverse,a,b))
$(info reading: $(X))
$(warning careful)
DEF := $(origin CC)|$(origin S)|$(origin NOPE)|$(origin CMD)|$(origin HOME)|$(origin MAKE)
override OV = 1
all:
	echo '[$(value X)] [$(value S)] [$(call reverse,one,two)] [$(call pair,a,b,c)]'
	echo '[$(call nested,q)] [$(DEF)|$(origin OV)|$(origin @)]'
	echo [$(foreach f,a b,$(call pair,$(f)))]
	echo $(info expanding) done
