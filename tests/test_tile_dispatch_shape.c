// test_tile_dispatch_shape.c - Phase F prep: tile-IR-native dispatch shape.
//
// Verifies tile_compute_dispatch_shape walks tile_root's TILE_AXIS
// children directly and computes (groups, threads) without going
// through KernelAxes -- the seam Phase F's renderer rewrite will
// use to flip the dispatch path.

#include "../src/thvm.c"
#include "test.h"

static u32 emit_axis(KernelEntry *ke, u32 kax_type, u32 extent) {
  TileAxisInfo info = { kax_type, extent, 0, 0 };
  return tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(info));
}

static u32 build_loop_nest(KernelEntry *ke, u32 store, u32 *axes,
                           u32 n_axes) {
  u32 src[TILE_MAX_SRC] = {store};
  for (u32 i = 0; i < n_axes; i++) src[1 + i] = axes[i];
  u32 root = tile_emit(ke, TILE_LOOP_NEST, DT_FP32,
                       (u8)(1 + n_axes), src, 0);
  ke->tile_root = root;
  return root;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("tile-dispatch-shape/null-args-reject");
  u32 g = 0, t = 0;
  CHECK_EQ(tile_compute_dispatch_shape(NULL, &g, &t), 0);

  KernelEntry ke = {0};
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 0);

  // Build a synthetic TILE_LOOP_NEST whose body is a TILE_STORE wrapping
  // a TILE_SCALAR_BODY (no real scalars wired -- we only check structure).
  u32 body = tile_emit_leaf(&ke, TILE_SCALAR_BODY, DT_FP32, 1);
  u32 store = tile_emit(&ke, TILE_STORE, DT_FP32, 1, &body, 0);

  TEST_BEGIN("tile-dispatch-shape/flat-grid-small");
  // 4 LOOP axes, total = 8*4 = 32 < 256 -> threads=32, groups=1.
  u32 ax_loop_a = emit_axis(&ke, KAX_LOOP, 8);
  u32 ax_loop_b = emit_axis(&ke, KAX_LOOP, 4);
  u32 axes_flat_small[2] = { ax_loop_a, ax_loop_b };
  build_loop_nest(&ke, store, axes_flat_small, 2);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 1);
  CHECK_EQ(t, 32u);
  CHECK_EQ(g, 1u);

  TEST_BEGIN("tile-dispatch-shape/flat-grid-large");
  // total = 1000 > 256 -> threads=256, groups=ceil(1000/256)=4.
  u32 ax_loop_big = emit_axis(&ke, KAX_LOOP, 1000);
  u32 axes_flat_big[1] = { ax_loop_big };
  build_loop_nest(&ke, store, axes_flat_big, 1);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 1);
  CHECK_EQ(t, 256u);
  CHECK_EQ(g, 4u);

  TEST_BEGIN("tile-dispatch-shape/local-global");
  // GLOBAL=64, LOCAL=128 -> groups=64, threads=128.
  u32 ax_global = emit_axis(&ke, KAX_GLOBAL, 64);
  u32 ax_local  = emit_axis(&ke, KAX_LOCAL, 128);
  u32 axes_lg[2] = { ax_global, ax_local };
  build_loop_nest(&ke, store, axes_lg, 2);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 1);
  CHECK_EQ(g, 64u);
  CHECK_EQ(t, 128u);

  TEST_BEGIN("tile-dispatch-shape/local-global-mixed-loop");
  // GLOBAL=8, LOCAL=64, plus a LOOP=4 axis multiplies into groups.
  u32 ax_g2  = emit_axis(&ke, KAX_GLOBAL, 8);
  u32 ax_l2  = emit_axis(&ke, KAX_LOCAL, 64);
  u32 ax_lp2 = emit_axis(&ke, KAX_LOOP, 4);
  u32 axes_lglp[3] = { ax_g2, ax_l2, ax_lp2 };
  build_loop_nest(&ke, store, axes_lglp, 3);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 1);
  CHECK_EQ(g, 32u);   // 8 * 4
  CHECK_EQ(t, 64u);

  TEST_BEGIN("tile-dispatch-shape/group-reduce");
  // GROUP_REDUCE=128, GLOBAL=16 -> groups=16, threads=128.
  u32 ax_gr  = emit_axis(&ke, KAX_GROUP_REDUCE, 128);
  u32 ax_g3  = emit_axis(&ke, KAX_GLOBAL, 16);
  u32 axes_gr[2] = { ax_gr, ax_g3 };
  build_loop_nest(&ke, store, axes_gr, 2);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 1);
  CHECK_EQ(g, 16u);
  CHECK_EQ(t, 128u);

  TEST_BEGIN("tile-dispatch-shape/group-reduce-too-large-rejects");
  // GROUP_REDUCE=512 > 256 hardware cap -> 0.
  u32 ax_gr_big = emit_axis(&ke, KAX_GROUP_REDUCE, 512);
  u32 axes_gr_big[1] = { ax_gr_big };
  build_loop_nest(&ke, store, axes_gr_big, 1);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 0);

  TEST_BEGIN("tile-dispatch-shape/zero-extent-rejects");
  u32 ax_zero = emit_axis(&ke, KAX_LOOP, 0);
  u32 axes_zero[1] = { ax_zero };
  build_loop_nest(&ke, store, axes_zero, 1);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 0);

  TEST_BEGIN("tile-dispatch-shape/reduce-axes-skip-grid");
  // KAX_REDUCE / KAX_UNROLL axes are in-kernel reductions; they
  // should NOT contribute to the dispatch grid.  LOOP=8 + REDUCE=16
  // -> total=8 (just LOOP), threads=8, groups=1.
  u32 ax_lp = emit_axis(&ke, KAX_LOOP, 8);
  u32 ax_rd = emit_axis(&ke, KAX_REDUCE, 16);
  u32 axes_red[2] = { ax_lp, ax_rd };
  build_loop_nest(&ke, store, axes_red, 2);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 1);
  CHECK_EQ(t, 8u);
  CHECK_EQ(g, 1u);

  TEST_BEGIN("tile-dispatch-shape/upcast-axes-counted");
  // KAX_UPCAST contributes to dispatch grid (it's an output axis).
  u32 ax_lp2u = emit_axis(&ke, KAX_LOOP, 4);
  u32 ax_up   = emit_axis(&ke, KAX_UPCAST, 4);
  u32 axes_lpup[2] = { ax_lp2u, ax_up };
  build_loop_nest(&ke, store, axes_lpup, 2);
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 1);
  CHECK_EQ(t, 16u);  // 4 * 4
  CHECK_EQ(g, 1u);

  TEST_BEGIN("tile-dispatch-shape/non-loop-nest-root-rejects");
  // tile_root pointing at TILE_STORE rather than TILE_LOOP_NEST.
  ke.tile_root = store;
  CHECK_EQ(tile_compute_dispatch_shape(&ke, &g, &t), 0);

  tile_free(&ke);
  thvm_free();
  TEST_REPORT();
}
