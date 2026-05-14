// test_tile_anno.c - Phase E: tile_anno_* axis read helpers.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("tile-anno/empty-axis-count-zero");
  CHECK_EQ(tile_anno_axis_count(ke), 0u);
  TileAxisInfo info;
  CHECK_EQ(tile_anno_axis_at(ke, 0, &info), 0);

  TEST_BEGIN("tile-anno/null-args-bail");
  CHECK_EQ(tile_anno_axis_count(NULL), 0u);
  CHECK_EQ(tile_anno_axis_at(ke, 0, NULL), 0);

  TEST_BEGIN("tile-anno/synthetic-loop-nest");
  // Build a synthetic TILE_LOOP_NEST(STORE, AXIS_LOOP_8, AXIS_REDUCE_4)
  // and verify the helpers read it.
  TileAxisInfo a0 = { KAX_LOOP,   8,  TILE_MEM_GLOBAL, 0 };
  TileAxisInfo a1 = { KAX_REDUCE, 4,  TILE_MEM_SHARED, 4 };
  u32 ax0 = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a0));
  u32 ax1 = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(a1));
  // Mock body: TILE_SCALAR_BODY then TILE_STORE.  No real scalar arena.
  u32 body  = tile_emit_leaf(ke, TILE_SCALAR_BODY, DT_FP32, 1);
  u32 store = tile_emit(ke, TILE_STORE, DT_FP32, 1, &body, 1);
  u32 srcs[3] = { store, ax0, ax1 };
  ke->tile_root = tile_emit(ke, TILE_LOOP_NEST, DT_FP32, 3, srcs, 0);
  CHECK_EQ(tile_anno_axis_count(ke), 2u);

  TEST_BEGIN("tile-anno/axis-info-round-trip");
  CHECK_EQ(tile_anno_axis_at(ke, 0, &info), 1);
  CHECK_EQ(info.kax_type, KAX_LOOP);
  CHECK_EQ(info.extent, 8u);
  CHECK_EQ(info.memory_scope, TILE_MEM_GLOBAL);
  CHECK_EQ(info.vector_width, 0u);

  CHECK_EQ(tile_anno_axis_at(ke, 1, &info), 1);
  CHECK_EQ(info.kax_type, KAX_REDUCE);
  CHECK_EQ(info.extent, 4u);
  CHECK_EQ(info.memory_scope, TILE_MEM_SHARED);
  CHECK_EQ(info.vector_width, 4u);

  TEST_BEGIN("tile-anno/out-of-range-bails");
  CHECK_EQ(tile_anno_axis_at(ke, 2, &info), 0);
  CHECK_EQ(tile_anno_axis_at(ke, 100, &info), 0);

  // E9: tile_anno_axis_set / tile_anno_axes_match / tile_anno_apply_split
  // were writer-side facades that mirrored axes_apply_opt's writes into
  // ke->schedule->axis_types[].  Deleted alongside the field.

  tile_free(ke);

  // full_shape + n_axes resolvers.  Build a kernel via the writer
  // trio (axes_default_for + axes_apply_opt UPCAST), then confirm
  // the resolver output matches the writer state on every axis.
  TEST_BEGIN("axes-resolve/default-shape");
  ke->schedule = &ke->_local_schedule;
  memset(ke->schedule, 0, sizeof(KpSchedule));
  ke->output_shape.ndim = 2;
  ke->output_shape.dims[0] = 8;
  ke->output_shape.dims[1] = 12;
  ke->output_numel = 96;
  axes_default_for(ke);
  CHECK_EQ(axes_resolve_n_axes(ke), 2u);
  u32 ext = 0;
  CHECK_EQ(axes_resolve_full_shape(ke, 0, &ext), 1u);
  CHECK_EQ(ext, 8u);
  CHECK_EQ(axes_resolve_full_shape(ke, 1, &ext), 1u);
  CHECK_EQ(ext, 12u);
  // Out-of-range read returns 0 with ext = 0.
  CHECK_EQ(axes_resolve_full_shape(ke, 5, &ext), 0u);
  CHECK_EQ(ext, 0u);

  TEST_BEGIN("axes-resolve/upcast-split");
  KOpt up = { KOP_UPCAST, 1, 4 };
  CHECK_EQ(kernel_apply_opt(ke, up), 1);
  CHECK_EQ(axes_resolve_n_axes(ke), 3u);
  CHECK_EQ(axes_resolve_full_shape(ke, 0, &ext), 1u);
  CHECK_EQ(ext, 8u);          // axis 0 untouched
  CHECK_EQ(axes_resolve_full_shape(ke, 1, &ext), 1u);
  CHECK_EQ(ext, 3u);          // 12/4 = 3
  CHECK_EQ(axes_resolve_full_shape(ke, 2, &ext), 1u);
  CHECK_EQ(ext, 4u);          // inner = arg

  TEST_BEGIN("axes-resolve/swap-exchange");
  KOpt sw = { KOP_SWAP, 0, 2 };
  CHECK_EQ(kernel_apply_opt(ke, sw), 1);
  CHECK_EQ(axes_resolve_full_shape(ke, 0, &ext), 1u);
  CHECK_EQ(ext, 4u);          // was inner
  CHECK_EQ(axes_resolve_full_shape(ke, 2, &ext), 1u);
  CHECK_EQ(ext, 8u);          // was axis 0

  TEST_BEGIN("tile-axes-hash/non-zero-and-stable");
  u64 h0 = tile_axes_hash(ke);
  CHECK(h0 != 0ULL);
  CHECK_EQ(tile_axes_hash(ke), h0);

  TEST_BEGIN("tile-axes-hash/changes-on-apply-opt");
  KOpt un = { KOP_UNROLL, 0, 2 };
  CHECK_EQ(kernel_apply_opt(ke, un), 1);
  u64 h1 = tile_axes_hash(ke);
  CHECK(h1 != h0);

  thvm_free();
  TEST_REPORT();
}
