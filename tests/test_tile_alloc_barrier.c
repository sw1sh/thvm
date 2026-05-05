// test_tile_alloc_barrier.c - Phase D2: TILE_LOCAL_ALLOC + TILE_BARRIER.
//
// The constructors emit Tile-IR nodes for threadgroup-shared
// allocations and synchronization barriers.  No producer or renderer
// uses them yet -- D3's reduce-broadcast lowering is the first
// caller.  This commit just lands the IR shape + tests.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("tile-alloc/heap-layout");
  KernelEntry ke = {0};
  u32 a = tile_emit_alloc(&ke, DT_FP32, TILE_MEM_SHARED, 256);
  CHECK(a > 0);
  CHECK_EQ(ke.tile_uops[a].op, TILE_LOCAL_ALLOC);
  CHECK_EQ(ke.tile_uops[a].dtype, DT_FP32);
  CHECK_EQ(ke.tile_uops[a].src_count, 0);

  TEST_BEGIN("tile-alloc/scope-and-n-elements-round-trip");
  TileAllocInfo info = tile_alloc_unpack(ke.tile_uops[a].extra);
  CHECK_EQ(info.scope, TILE_MEM_SHARED);
  CHECK_EQ(info.n_elements, 256);

  TEST_BEGIN("tile-alloc/different-scopes-distinct");
  u32 a_local = tile_emit_alloc(&ke, DT_FP32, TILE_MEM_LOCAL, 256);
  u32 a_reg   = tile_emit_alloc(&ke, DT_FP32, TILE_MEM_REGISTER, 256);
  CHECK(a_local != a);
  CHECK(a_reg != a_local);
  TileAllocInfo i_local = tile_alloc_unpack(ke.tile_uops[a_local].extra);
  TileAllocInfo i_reg   = tile_alloc_unpack(ke.tile_uops[a_reg].extra);
  CHECK_EQ(i_local.scope, TILE_MEM_LOCAL);
  CHECK_EQ(i_reg.scope,   TILE_MEM_REGISTER);

  TEST_BEGIN("tile-alloc/large-n-elements-fits");
  u32 a_big = tile_emit_alloc(&ke, DT_FP32, TILE_MEM_SHARED, 65536);
  TileAllocInfo i_big = tile_alloc_unpack(ke.tile_uops[a_big].extra);
  CHECK_EQ(i_big.n_elements, 65536);

  TEST_BEGIN("tile-barrier/heap-layout");
  u32 b = tile_emit_barrier(&ke, TILE_MEM_SHARED);
  CHECK(b > 0);
  CHECK_EQ(ke.tile_uops[b].op, TILE_BARRIER);
  CHECK_EQ(ke.tile_uops[b].src_count, 0);
  CHECK_EQ((u32)ke.tile_uops[b].extra, TILE_MEM_SHARED);

  TEST_BEGIN("tile-barrier/distinct-from-alloc");
  CHECK(b != a);
  CHECK(b != a_local);

  TEST_BEGIN("tile-op-name/covers-new-nodes");
  CHECK_EQ(strcmp(tile_op_name(TILE_LOCAL_ALLOC), "TILE_LOCAL_ALLOC"), 0);
  CHECK_EQ(strcmp(tile_op_name(TILE_BARRIER), "TILE_BARRIER"), 0);
  CHECK_EQ(strcmp(tile_op_name(TILE_LOAD), "TILE_LOAD"), 0);

  TEST_BEGIN("tile-load/heap-layout");
  // Build a TILE_LOAD reading from the TILE_LOCAL_ALLOC at `a`.
  // For the addr we just stash a TILE_SCALAR_BODY wrapping a fake
  // scalar id (no real scalar arena set up) -- the constructor
  // doesn't validate src targets, only that src_count is right.
  u32 addr = tile_emit_leaf(&ke, TILE_SCALAR_BODY, DT_INT64, 42);
  u32 ld   = tile_emit_load(&ke, DT_FP32, a, addr);
  CHECK(ld > 0);
  CHECK_EQ(ke.tile_uops[ld].op, TILE_LOAD);
  CHECK_EQ(ke.tile_uops[ld].dtype, DT_FP32);
  CHECK_EQ(ke.tile_uops[ld].src_count, 2);
  CHECK_EQ(ke.tile_uops[ld].src[0], a);
  CHECK_EQ(ke.tile_uops[ld].src[1], addr);

  TEST_BEGIN("tile-load/distinct-from-alloc-and-barrier");
  CHECK(ld != a);
  CHECK(ld != b);

  tile_free(&ke);
  thvm_free();
  TEST_REPORT();
}
