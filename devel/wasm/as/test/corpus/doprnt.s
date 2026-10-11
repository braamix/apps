	.globaltype	__stack_pointer, i32
	.tabletype	__indirect_function_table, funcref
	.functype	emit (i32) -> ()
	.functype	emit_pad (i32, i32) -> ()
	.functype	emit_str (i32) -> ()
	.functype	ksprintn (i32, i64, i32, i32, i32, i32) -> (i32)
	.functype	__doprnt (i32, i32, i32, i32, i32) -> (i32)
	.functype	cvtround (f64, i32, i32, i32, i32, i32, i32) -> ()
	.functype	exponent (i32, i32, i32) -> (i32)
	.functype	cvt (f64, i32, i32, i32, i32, i32, i32, i32) -> (i32)
	.functype	modf (f64, i32) -> (f64)
	.functype	putbyte (i32) -> ()
	.section	.custom_section.target_features,"",@
	.int8	8
	.int8	43
	.int8	11
	.ascii	"bulk-memory"
	.int8	43
	.int8	15
	.ascii	"bulk-memory-opt"
	.int8	43
	.int8	22
	.ascii	"call-indirect-overlong"
	.int8	43
	.int8	10
	.ascii	"multivalue"
	.int8	43
	.int8	15
	.ascii	"mutable-globals"
	.int8	43
	.int8	19
	.ascii	"nontrapping-fptoint"
	.int8	43
	.int8	15
	.ascii	"reference-types"
	.int8	43
	.int8	8
	.ascii	"sign-ext"
	.type	g_to_buf,@object
	.section	.bss.g_to_buf,"",@
	.p2align	2, 0x0
g_to_buf:
	.skip	4
	.size	g_to_buf, 4
	.type	g_buf,@object
	.section	.bss.g_buf,"",@
	.p2align	2, 0x0
g_buf:
	.skip	4
	.size	g_buf, 4
	.type	g_size,@object
	.section	.bss.g_size,"",@
	.p2align	2, 0x0
g_size:
	.skip	4
	.size	g_size, 4
	.type	g_len,@object
	.section	.bss.g_len,"",@
	.p2align	2, 0x0
g_len:
	.skip	4
	.size	g_len, 4
	.section	.text.emit,"",@
	.type	emit,@function
emit:
	.functype	emit (i32) -> ()
	i32.const	0
	i32.load	g_to_buf
	i32.eqz
	if
	local.get	0
	call	putbyte
	else
	i32.const	0
	i32.load	g_len
	i32.const	0
	i32.load	g_size
	i32.const	1
	i32.sub
	i32.ge_s
	if
	else
	i32.const	0
	i32.load	g_buf
	i32.const	0
	i32.load	g_len
	i32.add
	local.get	0
	i32.extend8_s
	i32.store8	0
	end_if
	end_if
	i32.const	0
	i32.const	0
	i32.load	g_len
	i32.const	1
	i32.add
	i32.store	g_len
	end_function
	.section	.text.emit_pad,"",@
	.type	emit_pad,@function
emit_pad:
	.functype	emit_pad (i32, i32) -> ()
	block
	local.get	1
	i32.const	0
	i32.le_s
	br_if	0
	loop
	local.get	0
	call	emit
	local.get	1
	i32.const	1
	i32.sub
	local.tee	1
	local.tee	1
	i32.const	0
	i32.gt_s
	br_if	0
	end_loop
	end_block
	end_function
	.section	.text.emit_str,"",@
	.type	emit_str,@function
emit_str:
	.functype	emit_str (i32) -> ()
	block
	local.get	0
	i32.load8_s	0
	i32.eqz
	br_if	0
	loop
	local.get	0
	local.get	0
	i32.const	1
	i32.add
	local.set	0
	i32.load8_s	0
	call	emit
	local.get	0
	i32.load8_s	0
	br_if	0
	end_loop
	end_block
	end_function
	.type	_str1,@object
	.section	.rodata._str1,"",@
_str1:
	.ascii	"0123456789abcdef"
	.int8	0
	.size	_str1, 17
	.type	_str0,@object
	.section	.rodata._str0,"",@
_str0:
	.ascii	"0123456789ABCDEF"
	.int8	0
	.size	_str0, 17
	.section	.text.ksprintn,"",@
	.type	ksprintn,@function
ksprintn:
	.functype	ksprintn (i32, i64, i32, i32, i32, i32) -> (i32)
	.local	i32, i64, i32
	local.get	4
	i32.eqz
	if
	i32.const	_str1
	local.set	4
	else
	i32.const	_str0
	local.set	4
	end_if
	local.get	0
	local.set	6
	local.get	0
	i32.const	0
	i32.store8	0
	loop
	local.get	2
	i64.extend_i32_u
	local.set	7
	local.get	1
	local.get	7
	i64.rem_u
	i32.wrap_i64
	local.set	8
	local.get	6
	i32.const	1
	i32.add
	local.tee	6
	local.tee	6
	local.get	4
	local.get	8
	i32.add
	i32.load8_s	0
	i32.store8	0
	local.get	1
	local.get	7
	i64.div_u
	local.set	1
	local.get	3
	i32.const	1
	i32.sub
	local.tee	3
	local.tee	3
	i32.const	0
	i32.gt_s
	br_if	0
	local.get	1
	i64.eqz
	i32.eqz
	br_if	0
	local.get	5
	local.get	6
	local.get	0
	i32.sub
	i32.store	0
	local.get	6
	return
	end_loop
	unreachable
	end_function
	.type	_str6,@object
	.section	.rodata._str6,"",@
_str6:
	.ascii	"inf"
	.int8	0
	.size	_str6, 4
	.type	_str5,@object
	.section	.rodata._str5,"",@
_str5:
	.ascii	"INF"
	.int8	0
	.size	_str5, 4
	.type	_str4,@object
	.section	.rodata._str4,"",@
_str4:
	.ascii	"nan"
	.int8	0
	.size	_str4, 4
	.type	_str3,@object
	.section	.rodata._str3,"",@
_str3:
	.ascii	"NAN"
	.int8	0
	.size	_str3, 4
	.type	_str2,@object
	.section	.rodata._str2,"",@
_str2:
	.ascii	"(null)"
	.int8	0
	.size	_str2, 7
	.functype	__trunctfdf2 (i64, i64) -> (f64)
	.section	.text.__doprnt,"",@
	.globl	__doprnt
	.type	__doprnt,@function
__doprnt:
	.functype	__doprnt (i32, i32, i32, i32, i32) -> (i32)
	.local	i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i64, f64, i32
	global.get	__stack_pointer
	i32.const	432
	i32.sub
	local.tee	18
	global.set	__stack_pointer
	i32.const	0
	local.get	4
	i32.store	g_to_buf
	i32.const	0
	local.get	2
	i32.store	g_buf
	i32.const	0
	local.get	3
	i32.store	g_size
	i32.const	0
	i32.const	0
	i32.store	g_len
	i32.const	0
	local.set	5
	loop
	local.get	0
	local.get	5
	i32.add
	local.set	2
	loop
	local.get	2
	i32.load8_s	0
	local.tee	3
	i32.const	37
	i32.ne
	if
	local.get	3
	if
	local.get	3
	call	emit
	local.get	5
	i32.const	1
	i32.add
	local.set	5
	local.get	2
	i32.const	1
	i32.add
	local.set	2
	br	2
	else
	block
	i32.const	0
	i32.load	g_to_buf
	i32.eqz
	br_if	0
	i32.const	0
	i32.load	g_size
	i32.const	0
	i32.le_s
	br_if	0
	i32.const	0
	i32.load	g_len
	local.set	9
	i32.const	0
	i32.load	g_size
	i32.const	1
	i32.sub
	local.set	0
	i32.const	0
	i32.load	g_len
	local.get	0
	i32.le_s
	if
	else
	local.get	0
	local.set	9
	end_if
	i32.const	0
	i32.load	g_buf
	local.get	9
	i32.add
	i32.const	0
	i32.store8	0
	end_block
	i32.const	0
	i32.load	g_len
	local.get	18
	i32.const	432
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	5
	i32.const	1
	i32.add
	local.set	5
	i32.const	32
	local.set	15
	i32.const	0
	local.set	10
	i32.const	0
	local.set	14
	i32.const	0
	local.set	6
	i32.const	0
	local.set	4
	local.get	18
	i32.const	0
	i32.store	384
	i32.const	0
	local.set	12
	i32.const	0
	local.set	13
	i32.const	0
	local.set	7
	i32.const	-1
	local.set	11
	i32.const	0
	local.set	8
	loop
	local.get	0
	local.get	5
	i32.add
	i32.load8_s	0
	local.set	3
	local.get	5
	i32.const	1
	i32.add
	local.set	5
	local.get	3
	i32.const	46
	i32.ne
	if
	local.get	3
	i32.const	35
	i32.ne
	if
	local.get	3
	i32.const	43
	i32.ne
	if
	local.get	3
	i32.const	32
	i32.ne
	if
	local.get	3
	i32.const	45
	i32.ne
	if
	local.get	3
	i32.const	104
	i32.ne
	if
	local.get	3
	i32.const	108
	i32.ne
	if
	local.get	3
	i32.const	106
	i32.ne
	if
	block
	local.get	3
	i32.const	122
	i32.eq
	br_if	0
	local.get	3
	i32.const	116
	i32.eq
	br_if	0
	local.get	3
	i32.const	76
	i32.ne
	if
	local.get	3
	i32.const	42
	i32.ne
	if
	block
	local.get	3
	i32.const	48
	i32.ne
	br_if	0
	local.get	7
	br_if	0
	i32.const	48
	local.set	15
	br	12
	end_block
	block
	local.get	3
	i32.const	48
	i32.lt_s
	br_if	0
	local.get	3
	i32.const	57
	i32.gt_s
	br_if	0
	i32.const	0
	local.set	9
	local.get	0
	local.get	5
	i32.add
	local.set	2
	loop
	block
	local.get	9
	i32.const	10
	i32.mul
	local.get	3
	i32.add
	i32.const	48
	i32.sub
	local.set	9
	local.get	2
	i32.load8_s	0
	local.tee	3
	local.tee	3
	i32.const	48
	i32.lt_s
	br_if	0
	local.get	3
	i32.const	57
	i32.gt_s
	br_if	0
	local.get	5
	i32.const	1
	i32.add
	local.set	5
	local.get	2
	i32.const	1
	i32.add
	local.set	2
	br	1
	end_block
	local.get	7
	i32.eqz
	if
	local.get	9
	local.set	10
	else
	local.get	9
	local.set	11
	end_if
	br	13
	end_loop
	end_block
	block
	local.get	3
	i32.const	37
	i32.ne
	if
	local.get	3
	i32.const	99
	i32.ne
	if
	local.get	3
	i32.const	115
	i32.ne
	if
	block
	local.get	3
	i32.const	102
	i32.eq
	br_if	0
	local.get	3
	i32.const	70
	i32.eq
	br_if	0
	local.get	3
	i32.const	101
	i32.eq
	br_if	0
	local.get	3
	i32.const	69
	i32.eq
	br_if	0
	local.get	3
	i32.const	103
	i32.eq
	br_if	0
	local.get	3
	i32.const	71
	i32.eq
	br_if	0
	block
	block
	local.get	3
	i32.const	88
	i32.eq
	local.set	9
	local.get	3
	i32.const	100
	i32.eq
	br_if	0
	local.get	3
	i32.const	105
	i32.eq
	br_if	0
	block
	block
	local.get	3
	i32.const	117
	i32.eq
	local.tee	2
	br_if	0
	local.get	3
	i32.const	111
	i32.eq
	br_if	0
	local.get	3
	i32.const	120
	i32.eq
	br_if	0
	local.get	9
	br_if	0
	local.get	3
	i32.const	112
	i32.ne
	if
	i32.const	37
	call	emit
	local.get	3
	call	emit
	br	9
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i64.extend_i32_u
	local.set	16
	i32.const	16
	local.set	2
	i32.const	1
	local.set	4
	br	2
	end_if
	end_block
	local.get	8
	i32.const	3
	i32.ne
	if
	local.get	8
	i32.const	0
	i32.le_s
	if
	local.get	8
	i32.const	-1
	i32.ne
	if
	local.get	8
	i32.const	-2
	i32.ne
	if
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i64.extend_i32_u
	local.set	16
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i32.const	255
	i32.and
	i64.extend_i32_u
	local.set	16
	end_if
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i32.const	65535
	i32.and
	i64.extend_i32_u
	local.set	16
	end_if
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i64.extend_i32_u
	local.set	16
	end_if
	else
	local.get	1
	i32.const	8
	i32.add
	i32.const	1
	i32.sub
	i32.const	-8
	i32.and
	i32.const	8
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-8
	i32.add
	i64.load	0
	local.set	16
	end_if
	local.get	2
	i32.eqz
	if
	local.get	3
	i32.const	111
	i32.ne
	if
	i32.const	16
	local.set	2
	else
	i32.const	8
	local.set	2
	end_if
	else
	i32.const	10
	local.set	2
	end_if
	end_block
	i32.const	0
	local.set	12
	i32.const	0
	local.set	13
	br	1
	end_block
	local.get	8
	i32.const	3
	i32.ne
	if
	local.get	8
	i32.const	0
	i32.le_s
	if
	local.get	8
	i32.const	-1
	i32.ne
	if
	local.get	8
	i32.const	-2
	i32.ne
	if
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i64.extend_i32_s
	local.set	16
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i32.extend8_s
	i64.extend_i32_s
	local.set	16
	end_if
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i32.extend16_s
	i64.extend_i32_s
	local.set	16
	end_if
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i64.extend_i32_s
	local.set	16
	end_if
	else
	local.get	1
	i32.const	8
	i32.add
	i32.const	1
	i32.sub
	i32.const	-8
	i32.and
	i32.const	8
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-8
	i32.add
	i64.load	0
	local.set	16
	end_if
	local.get	12
	if
	else
	i32.const	1
	local.set	12
	end_if
	local.get	16
	i64.const	0
	i64.ge_s
	if
	else
	local.get	18
	i32.const	45
	i32.store	384
	i64.const	0
	local.get	16
	i64.sub
	local.set	16
	end_if
	i32.const	10
	local.set	2
	end_block
	local.get	18
	i32.load	384
	if
	else
	local.get	12
	i32.const	0
	i32.ge_s
	if
	block
	local.get	12
	i32.eqz
	br_if	0
	local.get	13
	i32.eqz
	br_if	0
	local.get	18
	i32.const	32
	i32.store	384
	end_block
	else
	local.get	18
	i32.const	43
	i32.store	384
	end_if
	end_if
	block
	local.get	7
	i32.eqz
	br_if	0
	local.get	15
	i32.const	48
	i32.ne
	br_if	0
	i32.const	32
	local.set	15
	end_block
	local.get	11
	i32.const	32
	i32.lt_s
	if
	else
	local.get	11
	i32.const	32
	i32.sub
	i32.const	1
	i32.add
	local.set	14
	i32.const	31
	local.set	11
	end_if
	block
	block
	local.get	7
	i32.eqz
	br_if	0
	local.get	11
	br_if	0
	local.get	16
	i64.eqz
	i32.eqz
	br_if	0
	local.get	18
	i32.const	0
	i32.store	388
	local.get	18
	local.set	3
	br	1
	end_block
	local.get	18
	local.get	16
	local.get	2
	local.get	11
	local.get	9
	local.get	18
	i32.const	388
	i32.add
	call	ksprintn
	local.set	3
	end_block
	block
	local.get	4
	i32.eqz
	br_if	0
	local.get	16
	i64.eqz
	br_if	0
	block
	block
	local.get	2
	i32.const	8
	i32.ne
	br_if	0
	local.get	11
	local.get	18
	i32.load	388
	i32.gt_s
	br_if	0
	local.get	18
	local.get	18
	i32.load	388
	i32.const	1
	i32.add
	i32.store	388
	br	1
	end_block
	local.get	2
	i32.const	16
	i32.ne
	if
	else
	local.get	18
	local.get	18
	i32.load	388
	i32.const	2
	i32.add
	i32.store	388
	end_if
	end_block
	end_block
	local.get	18
	i32.load	384
	i32.eqz
	if
	else
	local.get	18
	local.get	18
	i32.load	388
	i32.const	1
	i32.add
	i32.store	388
	end_if
	block
	local.get	6
	br_if	0
	local.get	15
	i32.const	32
	i32.ne
	br_if	0
	local.get	10
	local.get	18
	i32.load	388
	i32.sub
	local.tee	7
	i32.const	0
	i32.le_s
	br_if	0
	i32.const	32
	local.get	7
	call	emit_pad
	end_block
	local.get	18
	i32.load	384
	i32.eqz
	if
	else
	local.get	18
	i32.load	384
	call	emit
	end_if
	block
	local.get	4
	i32.eqz
	br_if	0
	local.get	16
	i64.eqz
	br_if	0
	block
	block
	local.get	2
	i32.const	8
	i32.ne
	br_if	0
	local.get	11
	local.get	18
	i32.load	388
	i32.gt_s
	br_if	0
	i32.const	48
	call	emit
	br	1
	end_block
	local.get	2
	i32.const	16
	i32.ne
	if
	else
	i32.const	48
	call	emit
	local.get	9
	i32.eqz
	if
	i32.const	120
	local.set	2
	else
	i32.const	88
	local.set	2
	end_if
	local.get	2
	call	emit
	end_if
	end_block
	end_block
	local.get	14
	i32.eqz
	if
	else
	i32.const	48
	local.get	14
	call	emit_pad
	end_if
	block
	local.get	6
	br_if	0
	local.get	15
	i32.const	48
	i32.ne
	br_if	0
	local.get	10
	local.get	18
	i32.load	388
	i32.sub
	local.tee	2
	i32.const	0
	i32.le_s
	br_if	0
	i32.const	48
	local.get	2
	call	emit_pad
	end_block
	block
	local.get	3
	local.get	18
	i32.le_u
	br_if	0
	loop
	local.get	3
	local.get	3
	i32.const	-1
	i32.add
	local.set	3
	i32.load8_s	0
	call	emit
	local.get	3
	local.get	18
	i32.gt_u
	br_if	0
	end_loop
	end_block
	block
	local.get	6
	i32.eqz
	br_if	0
	local.get	10
	local.get	18
	i32.load	388
	i32.sub
	local.tee	2
	i32.const	0
	i32.le_s
	br_if	0
	i32.const	32
	local.get	2
	call	emit_pad
	end_block
	br	4
	end_block
	local.get	8
	i32.const	2
	i32.ne
	if
	local.get	1
	i32.const	8
	i32.add
	i32.const	1
	i32.sub
	i32.const	-8
	i32.and
	i32.const	8
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-8
	i32.add
	f64.load	0
	local.set	17
	else
	local.get	1
	i32.const	16
	i32.add
	i32.const	1
	i32.sub
	i32.const	-16
	i32.and
	i32.const	16
	i32.add
	local.set	1
	local.get	18
	i32.const	400
	i32.add
	local.get	1
	i32.const	-16
	i32.add
	i32.const	16
	memory.copy	0, 0
	local.get	18
	i64.load	400
	local.get	18
	i64.load	408
	call	__trunctfdf2
	local.set	17
	end_if
	local.get	3
	i32.const	70
	i32.eq
	if
	i32.const	1
	local.set	2
	else
	local.get	3
	i32.const	69
	i32.eq
	local.set	2
	end_if
	local.get	2
	if
	i32.const	1
	local.set	2
	else
	local.get	3
	i32.const	71
	i32.eq
	local.set	2
	end_if
	block
	block
	local.get	17
	f64.const	0x0p+0
	f64.lt
	br_if	0
	block
	local.get	17
	f64.const	0x0p+0
	f64.eq
	i32.eqz
	br_if	0
	f64.const	0x1p+0
	local.get	17
	f64.div
	f64.const	0x0p+0
	f64.lt
	i32.eqz
	br_if	0
	br	1
	end_block
	br	1
	end_block
	local.get	18
	i32.const	1
	i32.store	384
	local.get	17
	f64.neg
	local.set	17
	end_block
	block
	local.get	17
	local.get	17
	f64.ne
	local.tee	7
	br_if	0
	local.get	17
	local.get	17
	f64.sub
	f64.const	0x0p+0
	f64.ne
	br_if	0
	local.get	11
	i32.const	17
	i32.le_s
	if
	local.get	11
	i32.const	-1
	i32.ne
	if
	else
	i32.const	6
	local.set	11
	end_if
	else
	block
	block
	block
	local.get	3
	i32.const	103
	i32.eq
	br_if	0
	local.get	3
	i32.const	71
	i32.ne
	br_if	1
	end_block
	local.get	4
	br_if	0
	br	1
	end_block
	local.get	11
	i32.const	17
	i32.sub
	local.set	14
	end_block
	i32.const	17
	local.set	11
	end_if
	block
	block
	local.get	17
	local.get	11
	local.get	4
	local.get	18
	i32.const	384
	i32.add
	local.get	3
	local.get	18
	i32.const	32
	i32.add
	i32.const	352
	local.get	18
	i32.const	392
	i32.add
	call	cvt
	local.set	2
	local.get	18
	i32.load	384
	br_if	0
	local.get	12
	i32.eqz
	br_if	0
	local.get	18
	i32.const	43
	i32.store	384
	br	1
	end_block
	block
	block
	local.get	18
	i32.load	384
	br_if	0
	local.get	13
	i32.eqz
	br_if	0
	local.get	18
	i32.const	32
	i32.store	384
	br	1
	end_block
	local.get	18
	i32.load	384
	i32.eqz
	if
	else
	local.get	18
	i32.const	45
	i32.store	384
	end_if
	end_block
	end_block
	local.get	18
	i32.load	384
	i32.eqz
	if
	i32.const	0
	local.set	3
	else
	i32.const	1
	local.set	3
	end_if
	block
	local.get	2
	local.get	3
	i32.add
	local.set	3
	local.get	18
	local.get	3
	i32.store	388
	local.get	6
	br_if	0
	local.get	15
	i32.const	32
	i32.ne
	br_if	0
	local.get	10
	local.get	3
	i32.sub
	local.tee	3
	i32.const	0
	i32.le_s
	br_if	0
	i32.const	32
	local.get	3
	call	emit_pad
	end_block
	local.get	18
	i32.load	384
	i32.eqz
	if
	else
	local.get	18
	i32.load	384
	call	emit
	end_if
	block
	local.get	6
	br_if	0
	local.get	15
	i32.const	48
	i32.ne
	br_if	0
	local.get	10
	local.get	18
	i32.load	388
	i32.sub
	local.tee	3
	i32.const	0
	i32.le_s
	br_if	0
	i32.const	48
	local.get	3
	call	emit_pad
	end_block
	block
	i32.const	0
	local.set	9
	local.get	2
	i32.const	0
	i32.le_s
	br_if	0
	loop
	block
	local.get	14
	i32.eqz
	br_if	0
	block
	local.get	18
	i32.const	32
	i32.add
	local.get	18
	i32.load	392
	local.get	9
	i32.add
	local.tee	3
	i32.add
	i32.load8_s	0
	i32.const	101
	i32.eq
	br_if	0
	local.get	18
	i32.const	32
	i32.add
	local.get	3
	i32.add
	i32.load8_s	0
	i32.const	69
	i32.ne
	br_if	1
	end_block
	i32.const	48
	local.get	14
	call	emit_pad
	i32.const	0
	local.set	14
	end_block
	local.get	18
	i32.const	32
	i32.add
	local.get	18
	i32.load	392
	local.get	9
	i32.add
	i32.add
	i32.load8_s	0
	call	emit
	local.get	9
	i32.const	1
	i32.add
	local.tee	9
	local.tee	9
	local.get	2
	i32.lt_s
	br_if	0
	end_loop
	end_block
	local.get	14
	i32.eqz
	if
	else
	i32.const	48
	local.get	14
	call	emit_pad
	end_if
	block
	local.get	6
	i32.eqz
	br_if	0
	local.get	10
	local.get	18
	i32.load	388
	i32.sub
	local.tee	2
	i32.const	0
	i32.le_s
	br_if	0
	i32.const	32
	local.get	2
	call	emit_pad
	end_block
	br	4
	end_block
	local.get	7
	i32.eqz
	if
	local.get	2
	i32.eqz
	if
	i32.const	_str6
	local.set	2
	else
	i32.const	_str5
	local.set	2
	end_if
	else
	local.get	2
	i32.eqz
	if
	i32.const	_str4
	local.set	2
	else
	i32.const	_str3
	local.set	2
	end_if
	end_if
	local.get	18
	i32.load	384
	if
	i32.const	1
	local.set	3
	else
	local.get	12
	i32.const	0
	i32.ne
	local.set	3
	end_if
	local.get	3
	if
	i32.const	1
	local.set	3
	else
	local.get	13
	i32.const	0
	i32.ne
	local.set	3
	end_if
	local.get	3
	i32.eqz
	if
	i32.const	0
	local.set	3
	else
	i32.const	1
	local.set	3
	end_if
	i32.const	3
	local.get	3
	i32.add
	local.set	3
	local.get	18
	local.get	3
	i32.store	388
	local.get	6
	if
	else
	i32.const	32
	local.get	10
	local.get	3
	i32.sub
	call	emit_pad
	end_if
	local.get	18
	i32.load	384
	i32.eqz
	if
	local.get	12
	i32.eqz
	if
	local.get	13
	i32.eqz
	if
	else
	i32.const	32
	call	emit
	end_if
	else
	i32.const	43
	call	emit
	end_if
	else
	i32.const	45
	call	emit
	end_if
	local.get	2
	call	emit_str
	local.get	6
	i32.eqz
	if
	else
	i32.const	32
	local.get	10
	local.get	18
	i32.load	388
	i32.sub
	call	emit_pad
	end_if
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	local.tee	3
	local.tee	3
	if
	else
	i32.const	_str2
	local.set	3
	end_if
	i32.const	0
	local.set	9
	local.get	3
	local.set	4
	local.get	3
	local.get	11
	i32.add
	local.set	2
	loop
	block
	block
	local.get	7
	i32.eqz
	br_if	0
	local.get	4
	local.get	2
	i32.ge_s
	br_if	1
	end_block
	local.get	4
	i32.load8_s	0
	i32.eqz
	br_if	0
	local.get	9
	i32.const	1
	i32.add
	local.set	9
	local.get	4
	i32.const	1
	i32.add
	local.set	4
	br	1
	end_block
	local.get	10
	local.get	9
	i32.sub
	local.set	4
	local.get	6
	if
	else
	i32.const	32
	local.get	4
	call	emit_pad
	end_if
	block
	local.get	3
	local.get	9
	i32.add
	local.set	2
	local.get	9
	i32.const	0
	i32.le_s
	br_if	0
	loop
	local.get	3
	i32.load8_s	0
	call	emit
	local.get	3
	i32.const	1
	i32.add
	local.tee	3
	local.get	2
	i32.lt_s
	br_if	0
	end_loop
	end_block
	local.get	6
	i32.eqz
	if
	else
	i32.const	32
	local.get	4
	call	emit_pad
	end_if
	end_loop
	end_if
	else
	local.get	6
	if
	else
	i32.const	32
	local.get	10
	i32.const	1
	i32.sub
	call	emit_pad
	end_if
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	i32.const	255
	i32.and
	call	emit
	local.get	6
	i32.eqz
	if
	else
	i32.const	32
	local.get	10
	i32.const	1
	i32.sub
	call	emit_pad
	end_if
	end_if
	else
	i32.const	37
	call	emit
	end_if
	end_block
	br	14
	else
	local.get	7
	if
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	local.set	11
	else
	local.get	1
	i32.const	4
	i32.add
	i32.const	1
	i32.sub
	i32.const	-4
	i32.and
	i32.const	4
	i32.add
	local.tee	1
	local.tee	1
	i32.const	-4
	i32.add
	i32.load	0
	local.tee	10
	local.tee	10
	i32.const	0
	i32.ge_s
	if
	else
	i32.const	1
	local.set	6
	i32.const	0
	local.get	10
	i32.sub
	local.set	10
	end_if
	end_if
	br	11
	end_if
	else
	i32.const	2
	local.set	8
	br	10
	end_if
	end_block
	i32.const	1
	local.set	8
	br	8
	else
	i32.const	3
	local.set	8
	br	8
	end_if
	else
	local.get	8
	i32.const	1
	i32.ne
	if
	i32.const	1
	local.set	8
	else
	i32.const	3
	local.set	8
	end_if
	br	7
	end_if
	else
	local.get	8
	i32.const	-1
	i32.ne
	if
	i32.const	-1
	local.set	8
	else
	i32.const	-2
	local.set	8
	end_if
	br	6
	end_if
	else
	i32.const	1
	local.set	6
	br	5
	end_if
	else
	i32.const	1
	local.set	13
	br	4
	end_if
	else
	i32.const	-1
	local.set	12
	br	3
	end_if
	else
	i32.const	1
	local.set	4
	br	2
	end_if
	else
	i32.const	1
	local.set	7
	i32.const	32
	local.set	15
	i32.const	0
	local.set	11
	br	1
	end_if
	end_loop
	end_if
	end_loop
	end_loop
	unreachable
	end_function
	.section	.text.cvtround,"",@
	.type	cvtround,@function
cvtround:
	.functype	cvtround (f64, i32, i32, i32, i32, i32, i32) -> ()
	.local	i32, i32, i32
	global.get	__stack_pointer
	i32.const	16
	i32.sub
	local.tee	9
	global.set	__stack_pointer
	local.get	2
	i32.load	0
	local.set	7
	local.get	0
	f64.const	0x0p+0
	f64.eq
	if
	local.get	4
	i32.const	48
	i32.sub
	local.set	4
	else
	local.get	0
	f64.const	0x1.4p+3
	f64.mul
	local.get	9
	call	modf
	f64.const	0x0p+0
	f64.eq
	local.set	5
	local.get	9
	f64.load	0
	i32.trunc_sat_f64_s
	local.set	4
	end_if
	local.get	3
	i32.load8_s	0
	i32.const	46
	i32.ne
	if
	local.get	3
	local.set	8
	else
	local.get	3
	i32.const	-1
	i32.add
	local.set	8
	end_if
	block
	block
	local.get	4
	i32.const	5
	i32.gt_s
	br_if	0
	block
	local.get	4
	i32.const	5
	i32.ne
	br_if	0
	block
	local.get	5
	i32.eqz
	br_if	0
	local.get	8
	i32.load8_s	0
	i32.const	48
	i32.sub
	i32.const	2
	i32.rem_s
	i32.eqz
	br_if	1
	end_block
	br	1
	end_block
	local.get	6
	i32.load	0
	i32.eqz
	if
	else
	loop
	local.get	3
	i32.load8_s	0
	i32.const	46
	i32.ne
	if
	else
	local.get	3
	i32.const	-1
	i32.add
	local.set	3
	end_if
	local.get	3
	i32.load8_s	0
	i32.const	48
	i32.eq
	if
	local.get	3
	local.get	7
	i32.ne
	if
	else
	local.get	6
	i32.const	0
	i32.store	0
	end_if
	local.get	3
	i32.const	-1
	i32.add
	local.set	3
	br	1
	end_if
	end_loop
	end_if
	br	1
	end_block
	loop
	local.get	3
	i32.load8_s	0
	i32.const	46
	i32.ne
	if
	else
	local.get	3
	i32.const	-1
	i32.add
	local.set	3
	end_if
	local.get	3
	local.get	3
	i32.load8_s	0
	i32.const	1
	i32.add
	i32.extend8_s
	i32.store8	0
	local.get	3
	i32.load8_s	0
	i32.const	57
	i32.gt_s
	if
	local.get	3
	i32.const	48
	i32.store8	0
	local.get	3
	local.get	7
	i32.ne
	if
	local.get	3
	i32.const	-1
	i32.add
	local.set	3
	br	2
	else
	local.get	1
	i32.eqz
	if
	local.get	3
	i32.const	-1
	i32.add
	i32.const	49
	i32.store8	0
	local.get	7
	i32.const	-1
	i32.add
	local.set	7
	else
	local.get	3
	i32.const	49
	i32.store8	0
	local.get	1
	local.get	1
	i32.load	0
	i32.const	1
	i32.add
	i32.store	0
	end_if
	end_if
	end_if
	end_loop
	end_block
	local.get	2
	local.get	7
	i32.store	0
	local.get	9
	i32.const	16
	i32.add
	global.set	__stack_pointer
	end_function
	.section	.text.exponent,"",@
	.type	exponent,@function
exponent:
	.functype	exponent (i32, i32, i32) -> (i32)
	.local	i32, i32
	global.get	__stack_pointer
	i32.const	16
	i32.sub
	local.tee	4
	global.set	__stack_pointer
	local.get	0
	i32.const	1
	i32.add
	local.set	3
	local.get	0
	local.get	2
	i32.extend8_s
	i32.store8	0
	local.get	1
	i32.const	0
	i32.ge_s
	if
	local.get	3
	i32.const	1
	i32.add
	local.set	0
	local.get	3
	i32.const	43
	i32.store8	0
	else
	i32.const	0
	local.get	1
	i32.sub
	local.set	1
	local.get	3
	i32.const	1
	i32.add
	local.set	0
	local.get	3
	i32.const	45
	i32.store8	0
	end_if
	i32.const	8
	local.set	2
	local.get	1
	i32.const	9
	i32.le_s
	if
	local.get	0
	i32.const	1
	i32.add
	local.set	2
	local.get	0
	i32.const	48
	i32.store8	0
	local.get	2
	i32.const	1
	i32.add
	local.set	0
	local.get	2
	local.get	1
	i32.const	48
	i32.add
	i32.extend8_s
	i32.store8	0
	else
	loop
	local.get	2
	i32.const	1
	i32.sub
	local.set	2
	local.get	1
	i32.const	10
	i32.rem_s
	i32.const	48
	i32.add
	i32.extend8_s
	local.set	3
	local.get	4
	local.get	2
	i32.add
	local.get	3
	i32.store8	0
	local.get	1
	i32.const	10
	i32.div_s
	local.tee	1
	local.tee	1
	i32.const	9
	i32.gt_s
	br_if	0
	block
	local.get	2
	i32.const	1
	i32.sub
	local.set	2
	local.get	4
	local.get	2
	i32.add
	local.get	1
	i32.const	48
	i32.add
	i32.extend8_s
	i32.store8	0
	local.get	2
	i32.const	8
	i32.ge_s
	br_if	0
	loop
	local.get	0
	local.get	0
	i32.const	1
	i32.add
	local.set	0
	local.get	4
	local.get	2
	i32.add
	i32.load8_s	0
	i32.store8	0
	local.get	2
	i32.const	1
	i32.add
	local.tee	2
	local.tee	2
	i32.const	8
	i32.lt_s
	br_if	0
	end_loop
	end_block
	end_loop
	end_if
	local.get	0
	local.get	4
	i32.const	16
	i32.add
	global.set	__stack_pointer
	end_function
	.section	.text.cvt,"",@
	.type	cvt,@function
cvt:
	.functype	cvt (f64, i32, i32, i32, i32, i32, i32, i32) -> (i32)
	.local	i32, i32, i32, i32, i32, i32, f64, i32, i32, i32
	global.get	__stack_pointer
	i32.const	32
	i32.sub
	local.tee	17
	global.set	__stack_pointer
	local.get	4
	i32.const	97
	i32.ge_s
	local.tee	11
	i32.eqz
	if
	else
	local.get	4
	i32.const	32
	i32.sub
	local.set	4
	end_if
	block
	local.get	17
	i32.const	0
	i32.store	20
	i32.const	0
	local.set	9
	i32.const	0
	local.set	10
	local.get	0
	local.get	17
	call	modf
	local.set	0
	local.get	5
	local.get	6
	i32.add
	i32.const	-1
	i32.add
	local.set	12
	local.get	5
	i32.const	1
	i32.add
	local.set	13
	local.get	17
	local.get	13
	i32.store	16
	local.get	13
	local.set	8
	local.get	12
	i32.const	-1
	i32.add
	local.set	6
	local.get	17
	f64.load	0
	f64.const	0x0p+0
	f64.eq
	br_if	0
	loop
	local.get	17
	f64.load	0
	f64.const	0x1.4p+3
	f64.div
	local.get	17
	call	modf
	local.set	14
	local.get	17
	local.get	14
	f64.store	8
	local.get	6
	local.get	14
	f64.const	0x1.47ae147ae147bp-7
	f64.add
	f64.const	0x1.4p+3
	f64.mul
	i32.trunc_sat_f64_s
	i32.const	48
	i32.add
	i32.extend8_s
	i32.store8	0
	local.get	6
	i32.const	-1
	i32.add
	local.set	6
	local.get	17
	local.get	17
	i32.load	20
	i32.const	1
	i32.add
	i32.store	20
	local.get	17
	f64.load	0
	f64.const	0x0p+0
	f64.ne
	br_if	0
	end_loop
	end_block
	local.get	4
	i32.const	70
	i32.ne
	if
	block
	local.get	4
	i32.const	69
	i32.eq
	br_if	0
	local.get	1
	if
	else
	local.get	1
	i32.const	1
	i32.add
	local.set	1
	end_if
	block
	local.get	17
	i32.load	20
	local.get	1
	i32.gt_s
	br_if	0
	block
	local.get	17
	i32.load	20
	br_if	0
	local.get	0
	f64.const	0x0p+0
	f64.eq
	br_if	0
	local.get	0
	f64.const	0x1.a36e2eb1c432dp-14
	f64.lt
	i32.eqz
	br_if	0
	br	1
	end_block
	local.get	17
	i32.load	20
	i32.eqz
	if
	local.get	13
	i32.const	1
	i32.add
	local.set	8
	local.get	13
	i32.const	48
	i32.store8	0
	else
	loop
	local.get	6
	i32.const	1
	i32.add
	local.tee	6
	local.tee	6
	local.get	12
	i32.lt_u
	if
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	local.get	6
	i32.load8_s	0
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.set	1
	br	1
	end_if
	end_loop
	end_if
	block
	block
	local.get	1
	br_if	0
	local.get	2
	br_if	0
	br	1
	end_block
	i32.const	1
	local.set	10
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	i32.const	46
	i32.store8	0
	end_block
	loop
	block
	local.get	1
	i32.eqz
	br_if	0
	local.get	0
	f64.const	0x0p+0
	f64.eq
	br_if	0
	local.get	0
	f64.const	0x1.4p+3
	f64.mul
	local.get	17
	i32.const	8
	i32.add
	call	modf
	local.set	0
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	local.get	17
	f64.load	8
	i32.trunc_sat_f64_s
	i32.const	48
	i32.add
	i32.extend8_s
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.set	1
	br	1
	end_block
	local.get	0
	f64.const	0x0p+0
	f64.eq
	if
	else
	local.get	0
	i32.const	0
	local.get	17
	i32.const	16
	i32.add
	local.get	8
	i32.const	-1
	i32.add
	i32.const	48
	i32.const	0
	local.get	3
	call	cvtround
	end_if
	local.get	2
	i32.eqz
	if
	local.get	10
	i32.eqz
	if
	else
	block
	local.get	8
	local.get	17
	i32.load	16
	i32.le_u
	br_if	0
	loop
	local.get	8
	i32.const	-1
	i32.add
	local.tee	8
	local.tee	8
	i32.load8_s	0
	i32.const	48
	i32.eq
	if
	local.get	8
	local.get	17
	i32.load	16
	i32.gt_u
	br_if	1
	end_if
	end_loop
	end_block
	local.get	8
	i32.load8_s	0
	i32.const	46
	i32.eq
	if
	else
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	end_if
	end_if
	else
	block
	local.get	1
	i32.const	0
	i32.le_s
	br_if	0
	loop
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	i32.const	48
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.tee	1
	local.tee	1
	i32.const	0
	i32.gt_s
	br_if	0
	end_loop
	end_block
	end_if
	local.get	7
	local.get	17
	i32.load	16
	local.get	5
	i32.sub
	i32.store	0
	local.get	8
	local.get	17
	i32.load	16
	i32.sub
	local.get	17
	i32.const	32
	i32.add
	global.set	__stack_pointer
	return
	end_loop
	end_block
	local.get	1
	i32.const	1
	i32.sub
	local.set	1
	i32.const	69
	local.set	4
	i32.const	1
	local.set	9
	end_block
	local.get	17
	i32.load	20
	i32.eqz
	if
	local.get	0
	f64.const	0x0p+0
	f64.eq
	if
	block
	block
	local.get	13
	i32.const	1
	i32.add
	local.tee	6
	local.set	8
	local.get	13
	i32.const	48
	i32.store8	0
	local.get	1
	br_if	0
	local.get	2
	br_if	0
	br	1
	end_block
	local.get	6
	i32.const	1
	i32.add
	local.set	8
	local.get	6
	i32.const	46
	i32.store8	0
	end_block
	else
	local.get	17
	i32.const	-1
	i32.store	20
	loop
	local.get	0
	f64.const	0x1.4p+3
	f64.mul
	local.get	17
	i32.const	8
	i32.add
	call	modf
	local.set	0
	local.get	17
	f64.load	8
	f64.const	0x0p+0
	f64.eq
	if
	local.get	17
	local.get	17
	i32.load	20
	i32.const	1
	i32.sub
	i32.store	20
	br	1
	else
	block
	block
	local.get	13
	i32.const	1
	i32.add
	local.tee	6
	local.set	8
	local.get	13
	local.get	17
	f64.load	8
	i32.trunc_sat_f64_s
	i32.const	48
	i32.add
	i32.extend8_s
	i32.store8	0
	local.get	1
	br_if	0
	local.get	2
	br_if	0
	br	1
	end_block
	local.get	6
	i32.const	1
	i32.add
	local.set	8
	local.get	6
	i32.const	46
	i32.store8	0
	end_block
	end_if
	end_loop
	end_if
	else
	block
	block
	local.get	6
	i32.const	1
	i32.add
	local.set	6
	local.get	13
	i32.const	1
	i32.add
	local.tee	10
	local.set	8
	local.get	13
	local.get	6
	i32.load8_s	0
	i32.store8	0
	local.get	1
	br_if	0
	local.get	2
	br_if	0
	br	1
	end_block
	local.get	10
	i32.const	1
	i32.add
	local.set	8
	local.get	10
	i32.const	46
	i32.store8	0
	end_block
	loop
	local.get	1
	if
	local.get	6
	i32.const	1
	i32.add
	local.tee	6
	local.tee	6
	local.get	12
	i32.lt_u
	if
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	local.get	6
	i32.load8_s	0
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.set	1
	br	2
	end_if
	end_if
	local.get	1
	if
	else
	local.get	6
	i32.const	1
	i32.add
	local.tee	6
	local.get	12
	i32.ge_u
	if
	else
	local.get	6
	i32.const	1
	i32.add
	local.set	10
	loop
	block
	local.get	10
	local.get	12
	i32.ge_u
	br_if	0
	local.get	10
	i32.load8_s	0
	i32.const	48
	i32.ne
	br_if	0
	local.get	10
	i32.const	1
	i32.add
	local.set	10
	br	1
	end_block
	local.get	17
	i32.const	20
	i32.add
	local.set	13
	local.get	17
	i32.const	16
	i32.add
	local.set	15
	local.get	8
	i32.const	-1
	i32.add
	local.set	16
	local.get	6
	i32.load8_s	0
	local.set	6
	local.get	10
	local.get	12
	i32.ne
	if
	i32.const	0
	local.set	10
	else
	local.get	0
	f64.const	0x0p+0
	f64.eq
	local.set	10
	end_if
	f64.const	0x0p+0
	local.get	13
	local.get	15
	local.get	16
	local.get	6
	local.get	10
	local.get	3
	call	cvtround
	f64.const	0x0p+0
	local.set	0
	end_loop
	end_if
	end_if
	local.get	17
	local.get	17
	i32.load	20
	i32.const	1
	i32.sub
	i32.store	20
	end_loop
	end_if
	local.get	0
	f64.const	0x0p+0
	f64.eq
	if
	else
	local.get	1
	i32.eqz
	if
	else
	loop
	block
	local.get	0
	f64.const	0x1.4p+3
	f64.mul
	local.get	17
	i32.const	8
	i32.add
	call	modf
	local.set	0
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	local.get	17
	f64.load	8
	i32.trunc_sat_f64_s
	i32.const	48
	i32.add
	i32.extend8_s
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.tee	1
	local.tee	1
	i32.eqz
	br_if	0
	local.get	0
	f64.const	0x0p+0
	f64.ne
	br_if	1
	end_block
	end_loop
	end_if
	local.get	0
	f64.const	0x0p+0
	f64.eq
	if
	else
	local.get	0
	local.get	17
	i32.const	20
	i32.add
	local.get	17
	i32.const	16
	i32.add
	local.get	8
	i32.const	-1
	i32.add
	i32.const	48
	i32.const	0
	local.get	3
	call	cvtround
	end_if
	end_if
	block
	local.get	1
	i32.const	0
	i32.le_s
	br_if	0
	loop
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	i32.const	48
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.tee	1
	local.tee	1
	i32.const	0
	i32.gt_s
	br_if	0
	end_loop
	end_block
	block
	local.get	9
	i32.eqz
	br_if	0
	local.get	2
	br_if	0
	block
	local.get	8
	local.get	17
	i32.load	16
	i32.le_u
	br_if	0
	loop
	local.get	8
	i32.const	-1
	i32.add
	local.tee	8
	local.tee	8
	i32.load8_s	0
	i32.const	48
	i32.eq
	if
	local.get	8
	local.get	17
	i32.load	16
	i32.gt_u
	br_if	1
	end_if
	end_loop
	end_block
	local.get	8
	i32.load8_s	0
	i32.const	46
	i32.ne
	if
	else
	local.get	8
	i32.const	-1
	i32.add
	local.set	8
	end_if
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	end_block
	local.get	11
	i32.eqz
	if
	else
	local.get	4
	i32.const	32
	i32.add
	local.set	4
	end_if
	local.get	8
	local.get	17
	i32.load	20
	local.get	4
	call	exponent
	local.get	7
	local.get	17
	i32.load	16
	local.get	5
	i32.sub
	i32.store	0
	local.get	17
	i32.load	16
	i32.sub
	local.get	17
	i32.const	32
	i32.add
	global.set	__stack_pointer
	return
	else
	local.get	17
	i32.load	20
	i32.eqz
	if
	local.get	13
	i32.const	1
	i32.add
	local.set	8
	local.get	13
	i32.const	48
	i32.store8	0
	else
	block
	local.get	6
	i32.const	1
	i32.add
	local.tee	6
	local.tee	6
	local.get	12
	i32.ge_u
	br_if	0
	loop
	local.get	6
	local.get	6
	i32.const	1
	i32.add
	local.set	6
	i32.load8_s	0
	local.set	4
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	local.get	4
	i32.store8	0
	local.get	6
	local.get	12
	i32.lt_u
	br_if	0
	end_loop
	end_block
	end_if
	block
	block
	local.get	1
	br_if	0
	local.get	2
	br_if	0
	br	1
	end_block
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	i32.const	46
	i32.store8	0
	end_block
	local.get	0
	f64.const	0x0p+0
	f64.eq
	if
	else
	local.get	1
	i32.eqz
	if
	else
	loop
	block
	local.get	0
	f64.const	0x1.4p+3
	f64.mul
	local.get	17
	i32.const	8
	i32.add
	call	modf
	local.set	0
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	local.get	17
	f64.load	8
	i32.trunc_sat_f64_s
	i32.const	48
	i32.add
	i32.extend8_s
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.tee	1
	local.tee	1
	i32.eqz
	br_if	0
	local.get	0
	f64.const	0x0p+0
	f64.ne
	br_if	1
	end_block
	end_loop
	end_if
	local.get	0
	f64.const	0x0p+0
	f64.eq
	if
	else
	local.get	0
	i32.const	0
	local.get	17
	i32.const	16
	i32.add
	local.get	8
	i32.const	-1
	i32.add
	i32.const	48
	i32.const	0
	local.get	3
	call	cvtround
	end_if
	end_if
	block
	local.get	1
	i32.const	0
	i32.le_s
	br_if	0
	loop
	local.get	8
	local.get	8
	i32.const	1
	i32.add
	local.set	8
	i32.const	48
	i32.store8	0
	local.get	1
	i32.const	1
	i32.sub
	local.tee	1
	local.tee	1
	i32.const	0
	i32.gt_s
	br_if	0
	end_loop
	end_block
	local.get	7
	local.get	17
	i32.load	16
	local.get	5
	i32.sub
	i32.store	0
	local.get	8
	local.get	17
	i32.load	16
	i32.sub
	local.get	17
	i32.const	32
	i32.add
	global.set	__stack_pointer
	return
	end_if
	unreachable
	end_function
