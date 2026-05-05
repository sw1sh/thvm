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

  // === TILE_BLOCK + canonical reduce-broadcast shape (D3) ===
  TEST_BEGIN("tile-block/heap-layout");
  // Build a synthetic 4-step block: alloc, body1, barrier, load.
  // The block's value is the LAST stmt (the load).
  u32 body1 = tile_emit_leaf(&ke, TILE_SCALAR_BODY, DT_FP32, 7);
  u32 stmts4[4] = { a, body1, b, ld };
  u32 blk = tile_emit_block(&ke, DT_FP32, stmts4, 4);
  CHECK(blk > 0);
  CHECK_EQ(ke.tile_uops[blk].op, TILE_BLOCK);
  CHECK_EQ(ke.tile_uops[blk].src_count, 4);
  CHECK_EQ(ke.tile_uops[blk].src[0], a);
  CHECK_EQ(ke.tile_uops[blk].src[1], body1);
  CHECK_EQ(ke.tile_uops[blk].src[2], b);
  CHECK_EQ(ke.tile_uops[blk].src[3], ld);

  TEST_BEGIN("tile-block/canonical-reduce-broadcast-shape");
  // The full pattern Phase D3 emits for BN-grad-style reduce-broadcast:
  //   TILE_BLOCK(
  //     TILE_LOCAL_ALLOC(SHARED, reduce_groups),
  //     TILE_REDUCE(... write into alloc),
  //     TILE_BARRIER(SHARED),
  //     TILE_LOAD(alloc, addr),
  //     TILE_SCALAR_BODY(post-reduce expression)
  //   )
  // Ensure the IR can hold all five and order is preserved.
  u32 alloc_rb = tile_emit_alloc(&ke, DT_FP32, TILE_MEM_SHARED, 32);
  u32 reduce_body = tile_emit_leaf(&ke, TILE_SCALAR_BODY, DT_FP32, 100);
  u32 reduce_into = tile_emit(&ke, TILE_REDUCE, DT_FP32, 1, &reduce_body, 0);
  u32 barr = tile_emit_barrier(&ke, TILE_MEM_SHARED);
  u32 load_addr_rb = tile_emit_leaf(&ke, TILE_SCALAR_BODY, DT_INT64, 200);
  u32 load_rb  = tile_emit_load(&ke, DT_FP32, alloc_rb, load_addr_rb);
  u32 post     = tile_emit_leaf(&ke, TILE_SCALAR_BODY, DT_FP32, 300);
  u32 stmts5[5] = { alloc_rb, reduce_into, barr, load_rb, post };
  u32 rb_block = tile_emit_block(&ke, DT_FP32, stmts5, 5);
  CHECK_EQ(ke.tile_uops[rb_block].op, TILE_BLOCK);
  CHECK_EQ(ke.tile_uops[rb_block].src_count, 5);
  CHECK_EQ(ke.tile_uops[rb_block].src[0], alloc_rb);
  CHECK_EQ(ke.tile_uops[rb_block].src[1], reduce_into);
  CHECK_EQ(ke.tile_uops[rb_block].src[2], barr);
  CHECK_EQ(ke.tile_uops[rb_block].src[3], load_rb);
  CHECK_EQ(ke.tile_uops[rb_block].src[4], post);

  TEST_BEGIN("tile-op-name/covers-block");
  CHECK_EQ(strcmp(tile_op_name(TILE_BLOCK), "TILE_BLOCK"), 0);

  TEST_BEGIN("tile-input-buf/heap-layout");
  // Phase F: TILE_INPUT_BUF leaf carrying input slot id.
  u32 in_buf = tile_emit_input_buf(&ke, DT_FP32, /*slot=*/3);
  CHECK(in_buf > 0);
  CHECK_EQ(ke.tile_uops[in_buf].op, TILE_INPUT_BUF);
  CHECK_EQ(ke.tile_uops[in_buf].dtype, DT_FP32);
  CHECK_EQ((u32)ke.tile_uops[in_buf].extra, 3u);
  CHECK_EQ(ke.tile_uops[in_buf].src_count, 0);

  TEST_BEGIN("tile-load/from-input-buf");
  // TILE_LOAD can read from a TILE_INPUT_BUF (global memory) just
  // like it reads from a TILE_LOCAL_ALLOC (shared memory).
  u32 addr2 = tile_emit_leaf(&ke, TILE_SCALAR_BODY, DT_INT64, 99);
  u32 ld_input = tile_emit_load(&ke, DT_FP32, in_buf, addr2);
  CHECK(ld_input > 0);
  CHECK_EQ(ke.tile_uops[ld_input].op, TILE_LOAD);
  CHECK_EQ(ke.tile_uops[ld_input].src[0], in_buf);

  TEST_BEGIN("tile-op-name/covers-input-buf");
  CHECK_EQ(strcmp(tile_op_name(TILE_INPUT_BUF), "TILE_INPUT_BUF"), 0);

  tile_free(&ke);
  thvm_free();
  TEST_REPORT();
}
