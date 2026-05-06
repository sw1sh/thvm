// src/aot/halloc.c
//
// Storage for the thread-local bump-allocator state declared in
// halloc.h.  __thread definitions live in a .c so the linker has
// exactly one copy.

#include "halloc.h"

__thread u64 aot_tl_chunk_lo = 0;
__thread u64 aot_tl_chunk_hi = 0;
