// test_tile_dump.c - tile-IR pretty-printer (Phase F prep).
//
// Captures tile_dump output into a memory buffer and asserts the
// presence of expected substrings for each node type.  Doesn't pin
// exact whitespace -- just verifies the dump covers each node and
// extracts the right metadata.

#include "../src/thvm.c"
#include "test.h"

static int contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

int main(void) {
  thvm_init();
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("tile-dump/empty-prints-empty-marker");
  char buf[1024];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  CHECK(fp != NULL);
  tile_dump(ke, fp);
  fclose(fp);
  CHECK(contains(buf, "<empty>"));

  TEST_BEGIN("tile-dump/reduce-broadcast-shape");
  // Build the canonical reduce-broadcast block and dump it.
  TileAxisInfo a_loop = { KAX_LOOP, 4, TILE_MEM_GLOBAL, 0 };
  TileAxisInfo a_red  = { KAX_REDUCE, 8, TILE_MEM_SHARED, 4 };
  u32 ax0 = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a_loop));
  u32 ax1 = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a_red));
  u32 alloc = tile_emit_alloc(ke, DT_FP32, TILE_MEM_SHARED, 32);
  u32 redbody = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_FP32, 7);
  u32 reduce = tile_emit(ke, TILE_REDUCE, DT_FP32, 1, &redbody, 11);
  u32 barr = tile_emit_barrier(ke, TILE_MEM_SHARED);
  u32 zero_addr = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_INT64, 0);
  u32 load = tile_emit_load(ke, DT_FP32, alloc, zero_addr);
  u32 post = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_FP32, 13);
  u32 stmts[5] = { alloc, reduce, barr, load, post };
  u32 block = tile_emit_block(ke, DT_FP32, stmts, 5);
  u32 store_src[1] = { block };
  u32 store = tile_emit(ke, TILE_STORE, DT_FP32, 1, store_src, 17);
  u32 lnest_src[3] = { store, ax0, ax1 };
  ke->tile_root = tile_emit(ke, TILE_LOOP_NEST, DT_FP32, 3, lnest_src, 0);

  char buf2[4096];
  fp = fmemopen(buf2, sizeof(buf2), "w");
  CHECK(fp != NULL);
  tile_dump(ke, fp);
  fclose(fp);
  // Smoke test the dump covers each node type and surfaces metadata.
  CHECK(contains(buf2, "tile_dump:"));
  CHECK(contains(buf2, "TILE_LOOP_NEST"));
  CHECK(contains(buf2, "TILE_STORE"));
  CHECK(contains(buf2, "TILE_BLOCK"));
  CHECK(contains(buf2, "TILE_LOCAL_ALLOC"));
  CHECK(contains(buf2, "scope=1"));     // SHARED == 1
  CHECK(contains(buf2, "n=32"));
  CHECK(contains(buf2, "TILE_REDUCE"));
  CHECK(contains(buf2, "TILE_BARRIER"));
  CHECK(contains(buf2, "TILE_LOAD"));
  CHECK(contains(buf2, "TILE_SCALAR_BODY"));
  CHECK(contains(buf2, "TILE_AXIS"));
  CHECK(contains(buf2, "LOOP"));
  CHECK(contains(buf2, "REDUCE"));
  CHECK(contains(buf2, "vw=4"));        // vector_width on a_red

  tile_free(ke);
  thvm_free();
  TEST_REPORT();
}
