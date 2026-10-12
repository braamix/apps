# The Braam process ABI (docs/Braam.md §7; braam-core test/system/abi.mjs): the
# five exports, which call the runtime in rt.c, and the two imports, kernel.sys and
# kernel.sys_async.  The memory is imported (wasm-ld --import-memory).  In assembly
# because the exports and imports need .export_name and .import_name, which vcc's C
# has no way to spell; crt0.o is this file.

	.functype	__braam_start (i32, i32) -> (i32)
	.functype	__braam_resume (i32, i32, i32) -> (i32)
	.functype	__braam_signal (i32) -> ()
	.functype	malloc (i32) -> (i32)
	.functype	free (i32) -> ()

	.functype	__braam_kernel_sys (i32, i32, i32, i32) -> (i32)
	.import_module	__braam_kernel_sys, kernel
	.import_name	__braam_kernel_sys, sys
	.functype	__braam_kernel_sys_async (i32, i32, i32, i32) -> ()
	.import_module	__braam_kernel_sys_async, kernel
	.import_name	__braam_kernel_sys_async, sys_async

# int _start(u32 argv, u32 len): 0 when the process has exited, 1 while a call waits.
	.section	.text._start,"",@
	.globl	_start
	.type	_start,@function
	.export_name	_start, _start
_start:
	.functype	_start (i32, i32) -> (i32)
	local.get	0
	local.get	1
	call	__braam_start
	end_function

# int _resume(u32 token, u32 reply, u32 len)
	.section	.text._resume,"",@
	.globl	_resume
	.type	_resume,@function
	.export_name	_resume, _resume
_resume:
	.functype	_resume (i32, i32, i32) -> (i32)
	local.get	0
	local.get	1
	local.get	2
	call	__braam_resume
	end_function

# u32 _alloc(u32 n): where the host may write n bytes.
	.section	.text._alloc,"",@
	.globl	_alloc
	.type	_alloc,@function
	.export_name	_alloc, _alloc
_alloc:
	.functype	_alloc (i32) -> (i32)
	local.get	0
	call	malloc
	end_function

# void _free(u32 ptr, u32 n)
	.section	.text._free,"",@
	.globl	_free
	.type	_free,@function
	.export_name	_free, _free
_free:
	.functype	_free (i32, i32) -> ()
	local.get	0
	call	free
	end_function

# void _sig(u32 n): a signal, recorded and nothing else.
	.section	.text._sig,"",@
	.globl	_sig
	.type	_sig,@function
	.export_name	_sig, _sig
_sig:
	.functype	_sig (i32) -> ()
	local.get	0
	call	__braam_signal
	end_function

# int braam_sys_sync(unsigned op, unsigned a0, unsigned a1, unsigned a2)
	.section	.text.braam_sys_sync,"",@
	.globl	braam_sys_sync
	.type	braam_sys_sync,@function
braam_sys_sync:
	.functype	braam_sys_sync (i32, i32, i32, i32) -> (i32)
	local.get	0
	local.get	1
	local.get	2
	local.get	3
	call	__braam_kernel_sys
	end_function

# void __braam_sys_async(unsigned op, unsigned token, const void *ptr, unsigned len)
	.section	.text.__braam_sys_async,"",@
	.globl	__braam_sys_async
	.type	__braam_sys_async,@function
__braam_sys_async:
	.functype	__braam_sys_async (i32, i32, i32, i32) -> ()
	local.get	0
	local.get	1
	local.get	2
	local.get	3
	call	__braam_kernel_sys_async
	end_function

# _Noreturn void exit(int status): Sys::Exit, then a trap: nothing unwinds.
	.section	.text.exit,"",@
	.globl	exit
	.type	exit,@function
exit:
	.functype	exit (i32) -> ()
	i32.const	1		# BRAAM_SYS_EXIT
	local.get	0
	i32.const	0
	i32.const	0
	call	__braam_kernel_sys
	drop
	unreachable
	end_function

# _Noreturn void abort(void): exit status 134 (128 + SIGABRT), as a shell reports it.
	.section	.text.abort,"",@
	.globl	abort
	.type	abort,@function
abort:
	.functype	abort () -> ()
	i32.const	134
	call	exit
	unreachable
	end_function
