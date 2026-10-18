// src/aot/halloc.c -- storage for halloc.h's per-thread chunk bounds.

#include "halloc.h"

__thread u64 aot_tl_chunk_lo = 0;
__thread u64 aot_tl_chunk_hi = 0;
