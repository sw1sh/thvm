// test_tile_render_msl.c - tile_render_msl_skeleton (Phase F prep).
//
// Captures pseudo-MSL output for the canonical reduce-broadcast
// shape and asserts the structural elements (kernel decl, axis
// loops, alloc decls, barrier, store comment) appear in the right
// nesting order.

#include "../src/thvm.c"
#include "test.h"

static int contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

int main(void) {
  thvm_init();
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("tile-render-msl/empty-prints-empty-marker");
  char buf[2048];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  CHECK(fp != NULL);
  tile_render_msl_skeleton(ke, fp);
  fclose(fp);
  CHECK(contains(buf, "<empty>"));

  TEST_BEGIN("tile-render-msl/loop-nest-with-store-and-axes");
  TileAxisInfo a_loop = { KAX_LOOP, 4, TILE_MEM_GLOBAL, 0 };
  TileAxisInfo a_red  = { KAX_REDUCE, 8, TILE_MEM_SHARED, 0 };
  u32 ax0 = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a_loop));
  u32 ax1 = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a_red));
  u32 body = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_FP32, 7);
  u32 store_src[1] = { body };
  u32 store = tile_emit(ke, TILE_STORE, DT_FP32, 1, store_src, 1);
  u32 lnest_src[3] = { store, ax0, ax1 };
  ke->tile_root = tile_emit(ke, TILE_LOOP_NEST, DT_FP32, 3, lnest_src, 0);

  char buf2[4096];
  fp = fmemopen(buf2, sizeof(buf2), "w");
  CHECK(fp != NULL);
  tile_render_msl_skeleton(ke, fp);
  fclose(fp);
  CHECK(contains(buf2, "kernel void tile_kernel"));
  CHECK(contains(buf2, "for (uint a0 = 0; a0 < 4"));
  CHECK(contains(buf2, "for (uint a1 = 0; a1 < 8"));
  CHECK(contains(buf2, "/*reduce*/"));
  CHECK(contains(buf2, "/* TILE_STORE S1 */"));
  CHECK(contains(buf2, "/* scalar body S7 */"));

  tile_free(ke);

  TEST_BEGIN("tile-render-msl/reduce-broadcast-block");
  // Build the canonical reduce-broadcast shape and verify each node
  // surfaces in the rendered output.
  TileAxisInfo a0 = { KAX_LOOP, 2, TILE_MEM_GLOBAL, 0 };
  ax0 = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a0));
  u32 alloc = tile_emit_alloc(ke, DT_FP32, TILE_MEM_SHARED, 32);
  u32 redbody = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_FP32, 100);
  u32 reduce = tile_emit(ke, TILE_REDUCE, DT_FP32, 1, &redbody, 11);
  u32 barr = tile_emit_barrier(ke, TILE_MEM_SHARED);
  u32 load_addr = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_INT64, 200);
  u32 load = tile_emit_load(ke, DT_FP32, alloc, load_addr);
  u32 post = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_FP32, 300);
  u32 stmts[5] = { alloc, reduce, barr, load, post };
  u32 block = tile_emit_block(ke, DT_FP32, stmts, 5);
  u32 store2_src[1] = { block };
  u32 store2 = tile_emit(ke, TILE_STORE, DT_FP32, 1, store2_src, 17);
  u32 lnest2_src[2] = { store2, ax0 };
  ke->tile_root = tile_emit(ke, TILE_LOOP_NEST, DT_FP32, 2, lnest2_src, 0);

  char buf3[8192];
  fp = fmemopen(buf3, sizeof(buf3), "w");
  CHECK(fp != NULL);
  tile_render_msl_skeleton(ke, fp);
  fclose(fp);
  CHECK(contains(buf3, "kernel void tile_kernel"));
  CHECK(contains(buf3, "for (uint a0 = 0; a0 < 2"));
  CHECK(contains(buf3, "/* TILE_BLOCK begin */"));
  CHECK(contains(buf3, "threadgroup float"));
  CHECK(contains(buf3, "[32]"));
  CHECK(contains(buf3, "threadgroup_barrier"));
  CHECK(contains(buf3, "/* TILE_REDUCE S11 */"));
  CHECK(contains(buf3, "/* TILE_LOAD */"));
  CHECK(contains(buf3, "/* scalar body S300 */"));
  CHECK(contains(buf3, "/* TILE_BLOCK end */"));
  CHECK(contains(buf3, "/* TILE_STORE S17 */"));

  tile_free(ke);

  TEST_BEGIN("tile-render-msl/load-from-input-buf");
  // TILE_INPUT_BUF + TILE_LOAD render as `in<slot>[/*addr*/]`.
  TileAxisInfo a_only = { KAX_LOOP, 4, TILE_MEM_GLOBAL, 0 };
  u32 ax_only = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a_only));
  u32 in_buf = tile_emit_input_buf(ke, DT_FP32, /*slot=*/2);
  u32 in_addr = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_INT64, 5);
  u32 in_load = tile_emit_load(ke, DT_FP32, in_buf, in_addr);
  u32 in_post = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_FP32, 99);
  u32 in_stmts[2] = { in_load, in_post };
  u32 in_block = tile_emit_block(ke, DT_FP32, in_stmts, 2);
  u32 in_store_src[1] = { in_block };
  u32 in_store = tile_emit(ke, TILE_STORE, DT_FP32, 1, in_store_src, 21);
  u32 in_lnest_src[2] = { in_store, ax_only };
  ke->tile_root = tile_emit(ke, TILE_LOOP_NEST, DT_FP32, 2, in_lnest_src, 0);

  char buf4[2048];
  fp = fmemopen(buf4, sizeof(buf4), "w");
  CHECK(fp != NULL);
  tile_render_msl_skeleton(ke, fp);
  fclose(fp);
  CHECK(contains(buf4, "in2[/*addr*/]"));
  CHECK(contains(buf4, "TILE_LOAD from input 2"));

  tile_free(ke);
  thvm_free();
  TEST_REPORT();
}
