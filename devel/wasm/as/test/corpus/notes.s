	.globaltype	__stack_pointer, i32
	.tabletype	__indirect_function_table, funcref
	.functype	__coro_push (i32, i32, i32, i32) -> (i32)
	.functype	fopen$resume (i32) -> (i32)
	.functype	__coro_setup (i32, i32, i32, i32, i32) -> (i32)
	.functype	fopen$init (i32, i32, i32) -> ()
	.functype	__coro_pop (i32, i32) -> ()
	.functype	fgets$resume (i32) -> (i32)
	.functype	fgets$init (i32, i32, i32, i32) -> ()
	.functype	fclose$resume (i32) -> (i32)
	.functype	fclose$init (i32, i32) -> ()
	.functype	stat$resume (i32) -> (i32)
	.functype	stat$init (i32, i32, i32) -> ()
	.functype	list$resume (i32) -> (i32)
	.functype	list$init (i32) -> ()
	.functype	list$initp (i32, i32) -> ()
	.functype	add$resume (i32) -> (i32)
	.functype	add$init (i32, i32, i32) -> ()
	.functype	remove$resume (i32) -> (i32)
	.functype	remove$init (i32, i32) -> ()
	.functype	rename$resume (i32) -> (i32)
	.functype	rename$init (i32, i32, i32) -> ()
	.functype	del$resume (i32) -> (i32)
	.functype	del$init (i32, i32) -> ()
	.functype	ask$resume (i32) -> (i32)
	.functype	ask$init (i32) -> ()
	.functype	ask$initp (i32, i32) -> ()
	.functype	main$resume (i32) -> (i32)
	.functype	main$init (i32, i32, i32) -> ()
	.functype	atoi (i32) -> (i32)
	.functype	fprintf (i32, i32, i32) -> (i32)
	.functype	fputc (i32, i32) -> (i32)
	.functype	fputs (i32, i32) -> (i32)
	.functype	perror (i32) -> ()
	.functype	printf (i32, i32) -> (i32)
	.functype	strchr (i32, i32) -> (i32)
	.functype	strcmp (i32, i32) -> (i32)
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
	.type	_str10,@object
	.section	.rodata._str10,"",@
_str10:
	.ascii	"(%d notes, %d bytes)\012"
	.int8	0
	.size	_str10, 22
	.type	_str9,@object
	.section	.rodata._str9,"",@
_str9:
	.ascii	"notes.txt"
	.int8	0
	.size	_str9, 10
	.type	_str8,@object
	.section	.rodata._str8,"",@
_str8:
	.ascii	"stat"
	.int8	0
	.size	_str8, 5
	.type	_str7,@object
	.section	.rodata._str7,"",@
_str7:
	.ascii	"fclose"
	.int8	0
	.size	_str7, 7
	.type	_str6,@object
	.section	.rodata._str6,"",@
_str6:
	.ascii	"fgets"
	.int8	0
	.size	_str6, 6
	.type	_str5,@object
	.section	.rodata._str5,"",@
_str5:
	.ascii	"%3d  %s"
	.int8	0
	.size	_str5, 8
	.type	_str4,@object
	.section	.rodata._str4,"",@
_str4:
	.ascii	"no notes\012"
	.int8	0
	.size	_str4, 10
	.type	_str3,@object
	.section	.rodata._str3,"",@
_str3:
	.ascii	"notes.txt"
	.int8	0
	.size	_str3, 10
	.type	_str2,@object
	.section	.rodata._str2,"",@
_str2:
	.ascii	"r"
	.int8	0
	.size	_str2, 2
	.type	_str1,@object
	.section	.rodata._str1,"",@
_str1:
	.ascii	"notes.txt"
	.int8	0
	.size	_str1, 10
	.type	_str0,@object
	.section	.rodata._str0,"",@
_str0:
	.ascii	"fopen"
	.int8	0
	.size	_str0, 6
	.section	.text.list$resume,"",@
	.type	list$resume,@function
list$resume:
	.functype	list$resume (i32) -> (i32)
	.local	i32, i32, i32, i32, i32, i32
	global.get	__stack_pointer
	i32.const	16
	i32.sub
	local.tee	6
	global.set	__stack_pointer
	block
	block
	block
	block
	block
	block
	block
	block
	block
	local.get	0
	i32.load	0
	br_table	{4, 3, 2, 1, 0, 4}
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	112
	local.get	1
	i32.const	2
	i32.ne
	if
	br	8
	else
	local.get	0
	i32.const	164
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	104
	i32.add
	local.tee	1
	i32.load	0
	call	stat$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	100
	local.get	1
	i32.const	2
	i32.ne
	if
	br	6
	else
	local.get	0
	i32.const	160
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	140
	i32.add
	local.tee	1
	i32.load	0
	call	fclose$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	2
	local.get	0
	local.get	2
	i32.store	136
	local.get	2
	i32.const	2
	i32.ne
	if
	i32.const	1
	local.set	1
	br	4
	else
	local.get	0
	i32.const	152
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	124
	i32.add
	local.tee	1
	i32.load	0
	call	fgets$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.tee	3
	i32.const	2
	i32.ne
	if
	br	2
	else
	local.get	0
	i32.const	116
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	132
	i32.add
	local.tee	1
	i32.load	0
	call	fopen$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	i32.const	fopen$co
	local.tee	4
	i32.load	0
	local.set	5
	local.get	0
	local.get	5
	local.get	4
	i32.load	4
	i32.const	_str0
	call	__coro_push
	local.set	3
	local.get	0
	i32.const	132
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	3
	local.get	5
	local.get	4
	i32.const	fopen$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	i32.const	_str1
	i32.const	_str2
	call	fopen$init
	i32.const	0
	local.set	3
	end_block
	local.get	0
	i32.const	132
	i32.add
	local.tee	5
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	116
	i32.add
	local.tee	4
	local.get	1
	i32.store	0
	local.get	1
	local.get	3
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	5
	i32.load	0
	call	fopen$resume
	local.set	3
	local.get	0
	i32.const	120
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	4
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	0
	i32.const	144
	i32.add
	local.tee	1
	local.get	5
	i32.load	0
	i32.load	28
	i32.store	0
	local.get	0
	local.get	5
	i32.load	0
	call	__coro_pop
	local.get	1
	i32.load	0
	if
	local.get	0
	i32.const	0
	i32.store	156
	local.get	0
	i32.const	fgets$co
	i32.store	148
	i32.const	0
	local.set	1
	else
	i32.const	0
	i32.load	errno
	i32.const	35
	i32.eq
	if
	i32.const	_str4
	i32.const	0
	call	printf
	drop
	local.get	0
	i32.const	0
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	else
	i32.const	_str3
	call	perror
	local.get	0
	i32.const	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_if
	else
	local.get	0
	local.get	5
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	1
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	loop
	block
	block
	local.get	1
	br_table	{0, 1, 0}
	end_block
	local.get	0
	i32.const	148
	i32.add
	local.tee	3
	i32.load	0
	local.tee	1
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	1
	i32.load	4
	i32.const	_str6
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	124
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.load	0
	i32.const	fgets$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	0
	i32.const	168
	i32.add
	i32.const	256
	local.get	0
	i32.load	144
	call	fgets$init
	i32.const	0
	local.set	2
	end_block
	local.get	0
	i32.const	124
	i32.add
	local.tee	5
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	152
	i32.add
	local.tee	4
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	5
	i32.load	0
	call	fgets$resume
	local.set	3
	local.get	0
	i32.const	128
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	4
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	5
	i32.load	0
	local.tee	1
	i32.load	28
	local.get	0
	local.get	1
	call	__coro_pop
	if
	i32.const	_str5
	local.get	0
	i32.const	156
	i32.add
	local.tee	1
	i32.load	0
	i32.const	1
	i32.add
	local.set	3
	local.get	1
	local.get	3
	i32.store	0
	local.get	6
	local.get	3
	i32.store	0
	local.get	6
	local.get	0
	i32.const	168
	i32.add
	i32.store	4
	local.get	6
	call	printf
	drop
	i32.const	0
	local.set	1
	br	2
	else
	i32.const	fclose$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str7
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	140
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	fclose$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	0
	i32.load	144
	call	fclose$init
	i32.const	0
	local.set	1
	end_if
	else
	local.get	0
	local.get	5
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	2
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_loop
	end_block
	local.get	0
	i32.const	140
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	160
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	fclose$resume
	local.set	2
	local.get	0
	i32.const	96
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	0
	local.get	4
	i32.load	0
	call	__coro_pop
	i32.const	stat$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str8
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	104
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	stat$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	i32.const	_str9
	local.get	0
	i32.const	32
	i32.add
	call	stat$init
	i32.const	0
	local.set	1
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	3
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	104
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	164
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	stat$resume
	local.set	2
	local.get	0
	i32.const	108
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.get	0
	local.get	1
	call	__coro_pop
	if
	else
	i32.const	_str10
	local.get	0
	i64.load	64
	i32.wrap_i64
	local.set	1
	local.get	6
	local.get	0
	i32.load	156
	i32.store	0
	local.get	6
	local.get	1
	i32.store	4
	local.get	6
	call	printf
	drop
	end_if
	local.get	0
	i32.const	0
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	4
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	unreachable
	end_function
	.section	.text.list$init,"",@
	.type	list$init,@function
list$init:
	.functype	list$init (i32) -> ()
	end_function
	.type	list$co,@object
	.section	.data.list$co,"",@
	.p2align	2, 0x0
list$co:
	.int32	424
	.int32	8
	.int32	list$initp
	.int32	list$resume
	.size	list$co, 16
	.section	.text.list$initp,"",@
	.type	list$initp,@function
list$initp:
	.functype	list$initp (i32, i32) -> ()
	end_function
	.type	_str17,@object
	.section	.rodata._str17,"",@
_str17:
	.ascii	"fclose"
	.int8	0
	.size	_str17, 7
	.type	_str16,@object
	.section	.rodata._str16,"",@
_str16:
	.ascii	"%s"
	.int8	0
	.size	_str16, 3
	.type	_str15,@object
	.section	.rodata._str15,"",@
_str15:
	.ascii	" %s"
	.int8	0
	.size	_str15, 4
	.type	_str14,@object
	.section	.rodata._str14,"",@
_str14:
	.ascii	"notes.txt"
	.int8	0
	.size	_str14, 10
	.type	_str13,@object
	.section	.rodata._str13,"",@
_str13:
	.ascii	"a"
	.int8	0
	.size	_str13, 2
	.type	_str12,@object
	.section	.rodata._str12,"",@
_str12:
	.ascii	"notes.txt"
	.int8	0
	.size	_str12, 10
	.type	_str11,@object
	.section	.rodata._str11,"",@
_str11:
	.ascii	"fopen"
	.int8	0
	.size	_str11, 6
	.section	.text.add$resume,"",@
	.type	add$resume,@function
add$resume:
	.functype	add$resume (i32) -> (i32)
	.local	i32, i32, i32, i32, i32, i32
	global.get	__stack_pointer
	i32.const	16
	i32.sub
	local.tee	6
	global.set	__stack_pointer
	block
	local.get	0
	i32.load	0
	local.tee	1
	i32.const	1
	i32.eq
	if
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.tee	2
	i32.const	2
	i32.ne
	if
	else
	local.get	0
	i32.const	44
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	40
	i32.add
	local.tee	1
	i32.load	0
	call	fopen$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	1
	i32.const	2
	i32.eq
	if
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	60
	local.get	1
	i32.const	2
	i32.ne
	if
	br	3
	else
	local.get	0
	i32.const	64
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	52
	i32.add
	local.tee	1
	i32.load	0
	call	fclose$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	i32.const	fopen$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str11
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	40
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	fopen$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	i32.const	_str12
	i32.const	_str13
	call	fopen$init
	i32.const	0
	local.set	2
	end_if
	end_if
	local.get	0
	i32.const	40
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	44
	i32.add
	local.tee	3
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	fopen$resume
	local.set	2
	local.get	0
	i32.const	48
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.set	4
	local.get	0
	local.get	1
	call	__coro_pop
	local.get	4
	if
	block
	i32.const	0
	local.set	5
	local.get	0
	i32.load	36
	local.set	2
	local.get	0
	i32.load	32
	local.set	1
	local.get	2
	local.get	1
	i32.const	2
	i32.shl
	i32.add
	local.set	3
	local.get	1
	i32.const	0
	i32.le_s
	br_if	0
	loop
	local.get	5
	i32.eqz
	if
	i32.const	_str16
	local.set	1
	else
	i32.const	_str15
	local.set	1
	end_if
	local.get	6
	local.get	2
	i32.load	0
	i32.store	0
	local.get	4
	local.get	1
	local.get	6
	call	fprintf
	drop
	local.get	5
	i32.const	1
	i32.add
	local.set	5
	local.get	2
	i32.const	4
	i32.add
	local.tee	2
	local.get	3
	i32.lt_s
	br_if	0
	end_loop
	end_block
	i32.const	10
	local.get	4
	call	fputc
	drop
	i32.const	fclose$co
	local.tee	3
	i32.load	0
	local.set	5
	local.get	0
	local.get	5
	local.get	3
	i32.load	4
	i32.const	_str17
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	52
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	5
	local.get	3
	i32.const	fclose$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	4
	call	fclose$init
	i32.const	0
	local.set	1
	else
	i32.const	_str14
	call	perror
	local.get	0
	i32.const	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	1
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	52
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	64
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	fclose$resume
	local.set	2
	local.get	0
	i32.const	56
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.get	0
	local.get	1
	call	__coro_pop
	if
	i32.const	1
	local.set	1
	else
	i32.const	0
	local.set	1
	end_if
	local.get	0
	local.get	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	2
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	unreachable
	end_function
	.section	.text.add$init,"",@
	.type	add$init,@function
add$init:
	.functype	add$init (i32, i32, i32) -> ()
	local.get	0
	local.get	1
	i32.store	32
	local.get	0
	local.get	2
	i32.store	36
	end_function
	.type	add$co,@object
	.section	.data.add$co,"",@
	.p2align	2, 0x0
add$co:
	.int32	68
	.int32	4
	.size	add$co, 8
	.type	_str35,@object
	.section	.rodata._str35,"",@
_str35:
	.ascii	"notes.txt"
	.int8	0
	.size	_str35, 10
	.type	_str34,@object
	.section	.rodata._str34,"",@
_str34:
	.ascii	"notes.new"
	.int8	0
	.size	_str34, 10
	.type	_str33,@object
	.section	.rodata._str33,"",@
_str33:
	.ascii	"rename"
	.int8	0
	.size	_str33, 7
	.type	_str32,@object
	.section	.rodata._str32,"",@
_str32:
	.ascii	"notes.new"
	.int8	0
	.size	_str32, 10
	.type	_str31,@object
	.section	.rodata._str31,"",@
_str31:
	.ascii	"remove"
	.int8	0
	.size	_str31, 7
	.type	_str30,@object
	.section	.rodata._str30,"",@
_str30:
	.ascii	"there is no note %d\012"
	.int8	0
	.size	_str30, 21
	.type	_str29,@object
	.section	.rodata._str29,"",@
_str29:
	.ascii	"fclose"
	.int8	0
	.size	_str29, 7
	.type	_str28,@object
	.section	.rodata._str28,"",@
_str28:
	.ascii	"fclose"
	.int8	0
	.size	_str28, 7
	.type	_str27,@object
	.section	.rodata._str27,"",@
_str27:
	.ascii	"fgets"
	.int8	0
	.size	_str27, 6
	.type	_str26,@object
	.section	.rodata._str26,"",@
_str26:
	.ascii	"fclose"
	.int8	0
	.size	_str26, 7
	.type	_str25,@object
	.section	.rodata._str25,"",@
_str25:
	.ascii	"notes.new"
	.int8	0
	.size	_str25, 10
	.type	_str24,@object
	.section	.rodata._str24,"",@
_str24:
	.ascii	"w"
	.int8	0
	.size	_str24, 2
	.type	_str23,@object
	.section	.rodata._str23,"",@
_str23:
	.ascii	"notes.new"
	.int8	0
	.size	_str23, 10
	.type	_str22,@object
	.section	.rodata._str22,"",@
_str22:
	.ascii	"fopen"
	.int8	0
	.size	_str22, 6
	.type	_str21,@object
	.section	.rodata._str21,"",@
_str21:
	.ascii	"notes.txt"
	.int8	0
	.size	_str21, 10
	.type	_str20,@object
	.section	.rodata._str20,"",@
_str20:
	.ascii	"r"
	.int8	0
	.size	_str20, 2
	.type	_str19,@object
	.section	.rodata._str19,"",@
_str19:
	.ascii	"notes.txt"
	.int8	0
	.size	_str19, 10
	.type	_str18,@object
	.section	.rodata._str18,"",@
_str18:
	.ascii	"fopen"
	.int8	0
	.size	_str18, 6
	.section	.text.del$resume,"",@
	.type	del$resume,@function
del$resume:
	.functype	del$resume (i32) -> (i32)
	.local	i32, i32, i32, i32, i32, i32
	global.get	__stack_pointer
	i32.const	16
	i32.sub
	local.tee	6
	global.set	__stack_pointer
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	block
	local.get	0
	i32.load	0
	br_table	{8, 7, 6, 5, 4, 3, 2, 1, 0, 8}
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	140
	local.get	1
	i32.const	2
	i32.ne
	if
	br	15
	else
	local.get	0
	i32.const	196
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	132
	i32.add
	local.tee	1
	i32.load	0
	call	rename$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	128
	local.get	1
	i32.const	2
	i32.ne
	if
	br	15
	else
	local.get	0
	i32.const	192
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	120
	i32.add
	local.tee	1
	i32.load	0
	call	remove$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	116
	local.get	1
	i32.const	2
	i32.ne
	if
	br	12
	else
	local.get	0
	i32.const	188
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	108
	i32.add
	local.tee	1
	i32.load	0
	call	fclose$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	104
	local.get	1
	i32.const	2
	i32.ne
	if
	br	10
	else
	local.get	0
	i32.const	184
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	96
	i32.add
	local.tee	1
	i32.load	0
	call	fclose$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	2
	local.get	0
	local.get	2
	i32.store	92
	local.get	2
	i32.const	2
	i32.ne
	if
	i32.const	1
	local.set	1
	br	8
	else
	local.get	0
	i32.const	164
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	84
	i32.add
	local.tee	1
	i32.load	0
	call	fgets$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	80
	local.get	1
	i32.const	2
	i32.ne
	if
	br	6
	else
	local.get	0
	i32.const	152
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	72
	i32.add
	local.tee	1
	i32.load	0
	call	fclose$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	1
	local.get	0
	local.get	1
	i32.store	68
	local.get	1
	i32.const	2
	i32.ne
	if
	br	4
	else
	local.get	0
	i32.const	148
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	60
	i32.add
	local.tee	1
	i32.load	0
	call	fopen$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.tee	3
	i32.const	2
	i32.ne
	if
	br	2
	else
	local.get	0
	i32.const	52
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	44
	i32.add
	local.tee	1
	i32.load	0
	call	fopen$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	i32.const	fopen$co
	local.set	1
	local.get	0
	i32.const	36
	i32.add
	local.tee	4
	local.get	1
	i32.store	0
	local.get	1
	i32.load	0
	local.set	5
	local.get	1
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	local.get	1
	i32.store	40
	local.get	0
	local.get	5
	local.get	1
	i32.load	0
	i32.const	_str18
	call	__coro_push
	local.set	1
	local.get	0
	i32.const	44
	i32.add
	local.tee	3
	local.get	1
	i32.store	0
	i32.const	fopen$resume
	local.set	1
	local.get	0
	local.get	1
	i32.store	48
	local.get	3
	i32.load	0
	local.get	5
	local.get	4
	i32.load	0
	local.get	1
	local.get	0
	call	__coro_setup
	drop
	local.get	3
	i32.load	0
	i32.const	_str19
	i32.const	_str20
	call	fopen$init
	i32.const	0
	local.set	3
	end_block
	local.get	0
	i32.const	44
	i32.add
	local.tee	5
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	52
	i32.add
	local.tee	4
	local.get	1
	i32.store	0
	local.get	1
	local.get	3
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	5
	i32.load	0
	call	fopen$resume
	local.set	3
	local.get	0
	i32.const	56
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	4
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	0
	i32.const	144
	i32.add
	local.tee	1
	local.get	5
	i32.load	0
	i32.load	28
	i32.store	0
	local.get	0
	local.get	5
	i32.load	0
	call	__coro_pop
	local.get	1
	i32.load	0
	if
	local.get	0
	i32.const	36
	i32.add
	local.tee	4
	i32.load	0
	i32.load	0
	local.set	5
	local.get	0
	local.get	5
	local.get	0
	i32.load	40
	i32.load	0
	i32.const	_str22
	call	__coro_push
	local.set	3
	local.get	0
	i32.const	60
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	3
	local.get	5
	local.get	4
	i32.load	0
	local.get	0
	i32.load	48
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	i32.const	_str23
	i32.const	_str24
	call	fopen$init
	i32.const	0
	local.set	1
	else
	i32.const	_str21
	call	perror
	local.get	0
	i32.const	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	0
	local.get	5
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	1
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	60
	i32.add
	local.tee	5
	i32.load	0
	i32.const	4
	i32.add
	local.set	3
	local.get	0
	i32.const	148
	i32.add
	local.tee	4
	local.get	3
	i32.store	0
	local.get	3
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	5
	i32.load	0
	call	fopen$resume
	local.set	3
	local.get	0
	i32.const	64
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	4
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	0
	i32.const	156
	i32.add
	local.tee	1
	local.get	5
	i32.load	0
	i32.load	28
	i32.store	0
	local.get	0
	local.get	5
	i32.load	0
	call	__coro_pop
	local.get	1
	i32.load	0
	if
	local.get	0
	i32.const	0
	i32.store	168
	local.get	0
	i32.const	fgets$co
	i32.store	160
	i32.const	0
	local.set	1
	br	3
	else
	i32.const	_str25
	call	perror
	i32.const	fclose$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str26
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	72
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	fclose$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	0
	i32.load	144
	call	fclose$init
	i32.const	0
	local.set	1
	end_if
	else
	local.get	0
	local.get	5
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	2
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	72
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	152
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	fclose$resume
	local.set	2
	local.get	0
	i32.const	76
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	0
	local.get	4
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	3
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	loop
	block
	block
	local.get	1
	br_table	{0, 1, 0}
	end_block
	local.get	0
	i32.const	160
	i32.add
	local.tee	3
	i32.load	0
	local.tee	1
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	1
	i32.load	4
	i32.const	_str27
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	84
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.load	0
	i32.const	fgets$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	0
	i32.const	200
	i32.add
	i32.const	256
	local.get	0
	i32.load	144
	call	fgets$init
	i32.const	0
	local.set	2
	end_block
	local.get	0
	i32.const	84
	i32.add
	local.tee	5
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	164
	i32.add
	local.tee	4
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	5
	i32.load	0
	call	fgets$resume
	local.set	3
	local.get	0
	i32.const	88
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	4
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	5
	i32.load	0
	local.tee	1
	i32.load	28
	local.get	0
	local.get	1
	call	__coro_pop
	if
	local.get	0
	i32.const	168
	i32.add
	local.tee	1
	i32.load	0
	i32.const	1
	i32.add
	local.set	3
	local.get	1
	local.get	3
	i32.store	0
	local.get	3
	local.get	0
	i32.load	32
	i32.eq
	if
	else
	local.get	0
	i32.const	200
	i32.add
	local.get	0
	i32.load	156
	call	fputs
	drop
	end_if
	i32.const	0
	local.set	1
	br	2
	else
	i32.const	fclose$co
	local.set	1
	local.get	0
	i32.const	172
	i32.add
	local.tee	3
	local.get	1
	i32.store	0
	local.get	1
	i32.load	0
	local.set	4
	local.get	1
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	local.get	1
	i32.store	176
	local.get	0
	local.get	4
	local.get	1
	i32.load	0
	i32.const	_str28
	call	__coro_push
	local.set	1
	local.get	0
	i32.const	96
	i32.add
	local.tee	2
	local.get	1
	i32.store	0
	i32.const	fclose$resume
	local.set	1
	local.get	0
	local.get	1
	i32.store	180
	local.get	2
	i32.load	0
	local.get	4
	local.get	3
	i32.load	0
	local.get	1
	local.get	0
	call	__coro_setup
	drop
	local.get	2
	i32.load	0
	local.get	0
	i32.load	144
	call	fclose$init
	i32.const	0
	local.set	1
	end_if
	else
	local.get	0
	local.get	5
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	4
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_loop
	end_block
	local.get	0
	i32.const	96
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	184
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	fclose$resume
	local.set	2
	local.get	0
	i32.const	100
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	0
	local.get	4
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	172
	i32.add
	local.tee	3
	i32.load	0
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	0
	i32.load	176
	i32.load	0
	i32.const	_str29
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	108
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.load	0
	local.get	0
	i32.load	180
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	0
	i32.load	156
	call	fclose$init
	i32.const	0
	local.set	1
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	5
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	108
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	188
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	fclose$resume
	local.set	2
	local.get	0
	i32.const	112
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	block
	local.get	0
	local.get	4
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.load	32
	local.tee	1
	i32.const	1
	i32.lt_s
	br_if	0
	local.get	1
	local.get	0
	i32.load	168
	i32.gt_s
	br_if	0
	i32.const	rename$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str33
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	132
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	rename$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	i32.const	_str34
	i32.const	_str35
	call	rename$init
	i32.const	0
	local.set	1
	br	2
	end_block
	i32.const	_str30
	local.get	6
	local.get	1
	i32.store	0
	local.get	6
	call	printf
	drop
	i32.const	remove$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str31
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	120
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	remove$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	i32.const	_str32
	call	remove$init
	i32.const	0
	local.set	1
	br	2
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	6
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	132
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	196
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	rename$resume
	local.set	2
	local.get	0
	i32.const	136
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.get	0
	local.get	1
	call	__coro_pop
	if
	i32.const	1
	local.set	1
	else
	i32.const	0
	local.set	1
	end_if
	local.get	0
	local.get	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	8
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	120
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	2
	local.get	0
	i32.const	192
	i32.add
	local.tee	3
	local.get	2
	i32.store	0
	local.get	2
	local.get	1
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	remove$resume
	local.set	2
	local.get	0
	i32.const	124
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	0
	local.get	4
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	7
	i32.store	0
	i32.const	0
	local.get	6
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	unreachable
	end_function
	.section	.text.del$init,"",@
	.type	del$init,@function
del$init:
	.functype	del$init (i32, i32) -> ()
	local.get	0
	local.get	1
	i32.store	32
	end_function
	.type	del$co,@object
	.section	.data.del$co,"",@
	.p2align	2, 0x0
del$co:
	.int32	456
	.int32	4
	.size	del$co, 8
	.type	_str39,@object
	.section	.rodata._str39,"",@
_str39:
	.ascii	"\012%d added\012"
	.int8	0
	.size	_str39, 11
	.type	_str38,@object
	.section	.rodata._str38,"",@
_str38:
	.ascii	"add"
	.int8	0
	.size	_str38, 4
	.type	_str37,@object
	.section	.rodata._str37,"",@
_str37:
	.ascii	"fgets"
	.int8	0
	.size	_str37, 6
	.type	_str36,@object
	.section	.rodata._str36,"",@
_str36:
	.ascii	"note? "
	.int8	0
	.size	_str36, 7
	.section	.text.ask$resume,"",@
	.type	ask$resume,@function
ask$resume:
	.functype	ask$resume (i32) -> (i32)
	.local	i32, i32, i32, i32, i32, i32, i32
	global.get	__stack_pointer
	i32.const	16
	i32.sub
	local.tee	7
	global.set	__stack_pointer
	local.get	0
	i32.load	0
	local.tee	1
	i32.const	1
	i32.eq
	if
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	6
	local.get	0
	local.get	6
	i32.store	76
	local.get	6
	i32.const	2
	i32.ne
	if
	i32.const	1
	local.set	1
	else
	local.get	0
	i32.const	44
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	40
	i32.add
	local.tee	1
	i32.load	0
	call	fgets$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	7
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	1
	i32.const	2
	i32.eq
	if
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.set	2
	local.get	0
	local.get	2
	i32.store	68
	local.get	2
	i32.const	2
	i32.ne
	if
	i32.const	2
	local.set	1
	else
	local.get	0
	i32.const	80
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	60
	i32.add
	local.tee	1
	i32.load	0
	call	add$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	local.get	7
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	0
	i32.const	0
	i32.store	72
	local.get	0
	i32.const	fgets$co
	i32.store	36
	local.get	0
	i32.const	add$co
	i32.store	56
	i32.const	0
	local.set	1
	end_if
	end_if
	loop
	block
	block
	block
	block
	local.get	1
	br_table	{0, 1, 2, 0}
	end_block
	i32.const	_str36
	i32.const	0
	call	printf
	drop
	local.get	0
	i32.const	36
	i32.add
	local.tee	4
	i32.load	0
	local.tee	1
	i32.load	0
	local.set	5
	local.get	0
	local.get	5
	local.get	1
	i32.load	4
	i32.const	_str37
	call	__coro_push
	local.set	3
	local.get	0
	i32.const	40
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	3
	local.get	5
	local.get	4
	i32.load	0
	i32.const	fgets$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	0
	i32.const	84
	i32.add
	i32.const	256
	i32.const	0
	i32.load	stdin
	call	fgets$init
	i32.const	0
	local.set	6
	end_block
	local.get	0
	i32.const	40
	i32.add
	local.tee	5
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	44
	i32.add
	local.tee	4
	local.get	1
	i32.store	0
	local.get	1
	local.get	6
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	5
	i32.load	0
	call	fgets$resume
	local.set	3
	local.get	0
	i32.const	48
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	4
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	5
	i32.load	0
	local.tee	1
	i32.load	28
	local.get	0
	local.get	1
	call	__coro_pop
	if
	local.get	0
	i32.const	84
	i32.add
	local.tee	3
	i32.const	10
	call	strchr
	local.set	1
	local.get	0
	local.get	1
	i32.store	52
	local.get	1
	i32.eqz
	if
	else
	local.get	1
	i32.const	0
	i32.store8	0
	end_if
	local.get	3
	i32.load8_s	0
	if
	local.get	0
	i32.const	32
	i32.add
	local.tee	4
	local.get	3
	i32.store	0
	local.get	0
	i32.const	56
	i32.add
	local.tee	3
	i32.load	0
	local.tee	1
	i32.load	0
	local.set	5
	local.get	0
	local.get	5
	local.get	1
	i32.load	4
	i32.const	_str38
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	60
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	5
	local.get	3
	i32.load	0
	i32.const	add$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	i32.const	1
	local.get	4
	call	add$init
	i32.const	0
	local.set	2
	else
	br	4
	end_if
	else
	i32.const	_str39
	local.get	7
	local.get	0
	i32.load	72
	i32.store	0
	local.get	7
	call	printf
	drop
	local.get	0
	i32.const	0
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	7
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	0
	local.get	5
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	1
	i32.store	0
	i32.const	0
	local.get	7
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	local.get	0
	i32.const	60
	i32.add
	local.tee	5
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	80
	i32.add
	local.tee	4
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	5
	i32.load	0
	call	add$resume
	local.set	3
	local.get	0
	i32.const	64
	i32.add
	local.tee	1
	local.get	3
	i32.store	0
	local.get	4
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	5
	i32.load	0
	local.tee	1
	i32.load	28
	local.get	0
	local.get	1
	call	__coro_pop
	i32.eqz
	if
	local.get	0
	i32.const	72
	i32.add
	local.tee	1
	local.get	1
	i32.load	0
	i32.const	1
	i32.add
	i32.store	0
	else
	local.get	0
	i32.const	1
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	local.get	7
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	else
	local.get	0
	local.get	5
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	2
	i32.store	0
	i32.const	0
	local.get	7
	i32.const	16
	i32.add
	global.set	__stack_pointer
	return
	end_if
	end_block
	i32.const	0
	local.set	1
	br	0
	end_loop
	unreachable
	end_function
	.section	.text.ask$init,"",@
	.type	ask$init,@function
ask$init:
	.functype	ask$init (i32) -> ()
	end_function
	.type	ask$co,@object
	.section	.data.ask$co,"",@
	.p2align	2, 0x0
ask$co:
	.int32	340
	.int32	4
	.int32	ask$initp
	.int32	ask$resume
	.size	ask$co, 16
	.section	.text.ask$initp,"",@
	.type	ask$initp,@function
ask$initp:
	.functype	ask$initp (i32, i32) -> ()
	end_function
	.type	_str47,@object
	.section	.rodata._str47,"",@
_str47:
	.ascii	"usage: notes [add words... | del n | ask]\012"
	.int8	0
	.size	_str47, 43
	.type	_str46,@object
	.section	.rodata._str46,"",@
_str46:
	.ascii	"ask"
	.int8	0
	.size	_str46, 4
	.type	_str45,@object
	.section	.rodata._str45,"",@
_str45:
	.ascii	"ask"
	.int8	0
	.size	_str45, 4
	.type	_str44,@object
	.section	.rodata._str44,"",@
_str44:
	.ascii	"del"
	.int8	0
	.size	_str44, 4
	.type	_str43,@object
	.section	.rodata._str43,"",@
_str43:
	.ascii	"del"
	.int8	0
	.size	_str43, 4
	.type	_str42,@object
	.section	.rodata._str42,"",@
_str42:
	.ascii	"add"
	.int8	0
	.size	_str42, 4
	.type	_str41,@object
	.section	.rodata._str41,"",@
_str41:
	.ascii	"add"
	.int8	0
	.size	_str41, 4
	.type	_str40,@object
	.section	.rodata._str40,"",@
_str40:
	.ascii	"list"
	.int8	0
	.size	_str40, 5
	.section	.text.main$resume,"",@
	.globl	main$resume
	.type	main$resume,@function
main$resume:
	.functype	main$resume (i32) -> (i32)
	.local	i32, i32, i32, i32, i32, i32
	block
	block
	block
	block
	block
	block
	block
	block
	block
	local.get	0
	i32.load	0
	br_table	{4, 3, 2, 1, 0, 4}
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.tee	2
	i32.const	2
	i32.ne
	if
	br	8
	else
	local.get	0
	i32.const	80
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	76
	i32.add
	local.tee	1
	i32.load	0
	call	ask$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.tee	2
	i32.const	2
	i32.ne
	if
	br	6
	else
	local.get	0
	i32.const	68
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	64
	i32.add
	local.tee	1
	i32.load	0
	call	del$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.tee	2
	i32.const	2
	i32.ne
	if
	br	4
	else
	local.get	0
	i32.const	56
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	52
	i32.add
	local.tee	1
	i32.load	0
	call	add$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	return
	end_if
	end_block
	local.get	0
	i32.load	4
	i32.const	1
	i32.shr_s
	local.tee	2
	i32.const	2
	i32.ne
	if
	br	2
	else
	local.get	0
	i32.const	44
	i32.add
	local.tee	2
	i32.load	0
	i32.const	5
	i32.store	0
	local.get	0
	i32.const	40
	i32.add
	local.tee	1
	i32.load	0
	call	list$resume
	drop
	local.get	2
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	0
	local.get	1
	i32.load	0
	call	__coro_pop
	local.get	0
	i32.const	-1
	i32.store	0
	i32.const	1
	return
	end_if
	end_block
	local.get	0
	i32.const	32
	i32.add
	local.tee	4
	i32.load	0
	i32.const	1
	i32.ne
	if
	block
	local.get	0
	i32.const	36
	i32.add
	local.tee	3
	i32.load	0
	i32.const	4
	i32.add
	local.tee	1
	i32.load	0
	i32.const	_str41
	call	strcmp
	br_if	0
	local.get	4
	i32.load	0
	i32.const	2
	i32.le_s
	br_if	0
	i32.const	add$co
	local.tee	5
	i32.load	0
	local.set	6
	local.get	0
	local.get	6
	local.get	5
	i32.load	4
	i32.const	_str42
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	52
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	6
	local.get	5
	i32.const	add$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	local.get	4
	i32.load	0
	i32.const	2
	i32.sub
	local.get	3
	i32.load	0
	i32.const	8
	i32.add
	call	add$init
	i32.const	0
	local.set	2
	br	3
	end_block
	block
	local.get	1
	i32.load	0
	i32.const	_str43
	call	strcmp
	br_if	0
	local.get	4
	i32.load	0
	i32.const	3
	i32.ne
	br_if	0
	i32.const	del$co
	local.tee	4
	i32.load	0
	local.set	5
	local.get	0
	local.get	5
	local.get	4
	i32.load	4
	i32.const	_str44
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	64
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	5
	local.get	4
	i32.const	del$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	3
	i32.load	0
	i32.load	8
	call	atoi
	local.set	2
	local.get	1
	i32.load	0
	local.get	2
	call	del$init
	i32.const	0
	local.set	2
	br	4
	end_block
	block
	local.get	1
	i32.load	0
	i32.const	_str45
	call	strcmp
	br_if	0
	local.get	4
	i32.load	0
	i32.const	2
	i32.ne
	br_if	0
	i32.const	ask$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str46
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	76
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	ask$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	call	ask$init
	i32.const	0
	local.set	2
	br	5
	end_block
	i32.const	0
	i32.load	stderr
	i32.const	_str47
	i32.const	0
	call	fprintf
	drop
	local.get	0
	i32.const	2
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	return
	else
	i32.const	list$co
	local.tee	3
	i32.load	0
	local.set	4
	local.get	0
	local.get	4
	local.get	3
	i32.load	4
	i32.const	_str40
	call	__coro_push
	local.set	2
	local.get	0
	i32.const	40
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	2
	local.get	4
	local.get	3
	i32.const	list$resume
	local.get	0
	call	__coro_setup
	drop
	local.get	1
	i32.load	0
	call	list$init
	i32.const	0
	local.set	2
	end_if
	end_block
	local.get	0
	i32.const	40
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	44
	i32.add
	local.tee	3
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	list$resume
	local.set	2
	local.get	0
	i32.const	48
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.set	2
	local.get	0
	local.get	1
	call	__coro_pop
	local.get	0
	local.get	2
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	1
	i32.store	0
	i32.const	0
	return
	end_if
	end_block
	local.get	0
	i32.const	52
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	56
	i32.add
	local.tee	3
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	add$resume
	local.set	2
	local.get	0
	i32.const	60
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.set	2
	local.get	0
	local.get	1
	call	__coro_pop
	local.get	0
	local.get	2
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	2
	i32.store	0
	i32.const	0
	return
	end_if
	end_block
	local.get	0
	i32.const	64
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	68
	i32.add
	local.tee	3
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	del$resume
	local.set	2
	local.get	0
	i32.const	72
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.set	2
	local.get	0
	local.get	1
	call	__coro_pop
	local.get	0
	local.get	2
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	3
	i32.store	0
	i32.const	0
	return
	end_if
	end_block
	local.get	0
	i32.const	76
	i32.add
	local.tee	4
	i32.load	0
	i32.const	4
	i32.add
	local.set	1
	local.get	0
	i32.const	80
	i32.add
	local.tee	3
	local.get	1
	i32.store	0
	local.get	1
	local.get	2
	i32.const	1
	i32.shl
	i32.const	1
	i32.or
	i32.store	0
	local.get	4
	i32.load	0
	call	ask$resume
	local.set	2
	local.get	0
	i32.const	84
	i32.add
	local.tee	1
	local.get	2
	i32.store	0
	local.get	3
	i32.load	0
	i32.const	0
	i32.store	0
	local.get	1
	i32.load	0
	if
	local.get	4
	i32.load	0
	local.tee	1
	i32.load	28
	local.set	2
	local.get	0
	local.get	1
	call	__coro_pop
	local.get	0
	local.get	2
	i32.store	28
	local.get	0
	i32.const	-2
	i32.store	0
	i32.const	1
	return
	else
	local.get	0
	local.get	4
	i32.load	0
	i32.load	24
	i32.store	24
	local.get	0
	i32.const	4
	i32.store	0
	i32.const	0
	return
	end_if
	unreachable
	end_function
	.section	.text.main$init,"",@
	.globl	main$init
	.type	main$init,@function
main$init:
	.functype	main$init (i32, i32, i32) -> ()
	local.get	0
	local.get	1
	i32.store	32
	local.get	0
	local.get	2
	i32.store	36
	end_function
	.type	main$co,@object
	.section	.data.main$co,"",@
	.globl	main$co
	.p2align	2, 0x0
main$co:
	.int32	88
	.int32	4
	.size	main$co, 8
