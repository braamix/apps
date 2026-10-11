# Loads and stores of every width, their offsets and alignments, symbols
# as offsets, and the bulk memory instructions.
	.section	.text.mem,"",@
	.globl	mem
	.type	mem,@function
mem:
	.functype	mem (i32) -> ()
	local.get	0
	local.get	0
	i32.load	0
	i32.store	4
	local.get	0
	local.get	0
	i64.load	8:p2align=2
	i64.store	16:p2align=3
	local.get	0
	local.get	0
	f32.load	24
	f32.store	28:p2align=0
	local.get	0
	local.get	0
	f64.load	32
	f64.store	40
	local.get	0
	local.get	0
	i32.load8_s	1
	i32.store8	2
	local.get	0
	local.get	0
	i32.load8_u	3
	i32.store16	4
	local.get	0
	local.get	0
	i32.load16_s	6
	i32.store16	8:p2align=0
	local.get	0
	local.get	0
	i32.load16_u	10
	drop
	drop
	local.get	0
	i64.load8_s	12
	local.get	0
	i64.load8_u	13
	i64.add
	local.get	0
	i64.load16_s	14
	i64.add
	local.get	0
	i64.load16_u	16
	i64.add
	local.get	0
	i64.load32_s	20
	i64.add
	local.get	0
	i64.load32_u	24
	i64.add
	drop
	local.get	0
	i64.const	1
	i64.store8	0
	local.get	0
	i64.const	1
	i64.store16	0
	local.get	0
	i64.const	1
	i64.store32	4294967295
	i32.const	0
	i32.load	buffer
	i32.const	0
	i32.load	buffer+8:p2align=2
	i32.add
	i32.const	0
	i32.load	buffer-4
	i32.add
	drop
	i32.const	0
	i32.const	5
	i32.store	buffer+12
	i32.const	0
	i32.const	6
	i32.store	buffer
	i32.const	0
	f32.const	0x1p+0
	f32.store	20
	i32.const	0
	i64.const	1
	i64.store	24
	memory.size	0
	memory.grow	0
	drop
	local.get	0
	i32.const	0
	i32.const	16
	memory.copy	0, 0
	local.get	0
	i32.const	0
	i32.const	16
	memory.fill	0
	end_function

	.type	buffer,@object
	.section	.bss.buffer,"",@
	.globl	buffer
	.p2align	4, 0x0
buffer:
	.skip	64
	.size	buffer, 64
