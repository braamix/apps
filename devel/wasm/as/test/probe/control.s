# Blocks of every result type, if and else, branches and br_table.
	.section	.text.control,"",@
	.globl	control
	.type	control,@function
control:
	.functype	control (i32) -> (i32)
	block
	loop
	local.get	0
	br_if	1
	local.get	0
	i32.const	1
	i32.sub
	local.set	0
	br	0
	end_loop
	end_block
	block	i64
	i64.const	1
	end_block
	drop
	block	f32
	f32.const	0x1p+0
	end_block
	drop
	block	f64
	f64.const	0x1p+0
	end_block
	drop
	block	void
	end_block
	i32.const	7
	block	(i32) -> (i32)
	end_block
	drop
	local.get	0
	if	i32
	i32.const	1
	else
	i32.const	2
	end_if
	drop
	local.get	0
	if
	nop
	end_if
	block
	block
	block
	local.get	0
	br_table	{0, 1, 2, 0}
	end_block
	end_block
	end_block
	local.get	0
	i32.eqz
	if
	unreachable
	end_if
	local.get	0
	end_function
