# Calls: to functions defined here, before and after their definitions, to
# imports, one with names of its own, and through the table; and functions'
# addresses taken in code and in data.
	.functype	ext (i32) -> (i32)
	.functype	host (i32, i32, i32, i32) -> (i32)
	.import_module	host, kernel
	.import_name	host, sys
	.functype	later () -> ()
	.functype	unused (f64) -> ()
	.functype	weakfn () -> (i32)

	.section	.text.caller,"",@
	.globl	caller
	.type	caller,@function
	.export_name	caller, run
caller:
	.functype	caller (i32) -> (i32)
	call	later
	local.get	0
	call	ext
	i32.const	1
	i32.const	2
	i32.const	3
	i32.const	4
	call	host
	i32.add
	i32.const	later
	call_indirect	() -> ()
	local.get	0
	i32.const	ext
	call_indirect	__indirect_function_table, (i32) -> (i32)
	i32.add
	call	weakfn
	i32.add
	end_function

	.section	.text.later,"",@
	.hidden	later
	.globl	later
	.type	later,@function
later:
	.functype	later () -> ()
	end_function

	.section	.text.weakfn,"",@
	.weak	weakfn
	.type	weakfn,@function
weakfn:
	.functype	weakfn () -> (i32)
	i32.const	weakfn
	end_function

	.type	table,@object
	.section	.data.table,"",@
	.p2align	2, 0x0
table:
	.int32	caller
	.int32	later
	.int32	ext
	.int32	0
	.size	table, 16
