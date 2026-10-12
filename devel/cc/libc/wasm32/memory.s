# The memory leaves of the wasm32 runtime: memcpy, memmove and memset are the bulk
# memory instructions (memory.copy handles an overlap, so memmove is memcpy), and the
# heap grows the linear memory.

# void *memcpy(void *dest, const void *src, size_t n)
	.section	.text.memcpy,"",@
	.globl	memcpy
	.type	memcpy,@function
memcpy:
	.functype	memcpy (i32, i32, i32) -> (i32)
	local.get	0
	local.get	1
	local.get	2
	memory.copy	0, 0
	local.get	0
	end_function

# void *memmove(void *dest, const void *src, size_t n)
	.section	.text.memmove,"",@
	.globl	memmove
	.type	memmove,@function
memmove:
	.functype	memmove (i32, i32, i32) -> (i32)
	local.get	0
	local.get	1
	local.get	2
	memory.copy	0, 0
	local.get	0
	end_function

# void *memset(void *s, int c, size_t n)
	.section	.text.memset,"",@
	.globl	memset
	.type	memset,@function
memset:
	.functype	memset (i32, i32, i32) -> (i32)
	local.get	0
	local.get	1
	local.get	2
	memory.fill	0
	local.get	0
	end_function

# unsigned long __vcc_memory_size(void): the linear memory's size in 64 KiB pages.
	.section	.text.__vcc_memory_size,"",@
	.globl	__vcc_memory_size
	.type	__vcc_memory_size,@function
__vcc_memory_size:
	.functype	__vcc_memory_size () -> (i32)
	memory.size	0
	end_function

# long __vcc_memory_grow(unsigned long pages): grow it by `pages`; the old size, or -1.
	.section	.text.__vcc_memory_grow,"",@
	.globl	__vcc_memory_grow
	.type	__vcc_memory_grow,@function
__vcc_memory_grow:
	.functype	__vcc_memory_grow (i32) -> (i32)
	local.get	0
	memory.grow	0
	end_function
