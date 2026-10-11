# Custom sections, in the order llvm writes them: its own after DATA, then
# linking and the relocations, then producers and target_features last.
	.section	.custom_section.target_features,"",@
	.int8	2
	.int8	43
	.int8	11
	.ascii	"bulk-memory"
	.int8	43
	.int8	8
	.ascii	"sign-ext"
	.section	.custom_section.producers,"",@
	.int8	1
	.int8	12
	.ascii	"processed-by"
	.int8	1
	.int8	3
	.ascii	"vcc"
	.int8	3
	.ascii	"1.0"
	.section	.custom_section.mine,"",@
	.ascii	"first"
	.section	.custom_section.yours,"",@
	.int32	12345
	.int64	-1

	.section	.text.f,"",@
	.globl	f
	.type	f,@function
f:
	.functype	f () -> ()
	end_function
