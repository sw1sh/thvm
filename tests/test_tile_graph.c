// test_tile_graph.c -- tile-plan arena above scalar-UOps.
//
// Builds the same tiny scalar graph as test_scalar_graph, then seeds
// a tile plan from it:
//   TILE_SCALAR_BODY(value expr id)
//   TILE_STORE(S_STORE id, body)
//   TILE_AXIS(...)
//   TILE_LOOP_NEST(store, axes...)
//
// The tile plan is non-dispatching scaffolding for future CPU/Metal
// tiled renderers.  This test pins arena lifecycle, name helpers, and
// the seed builder's two axis sources: scalar S_RANGE fallback and
// KernelAxes override.

#include "../src/thvm.c"
#include "test.h"

static u32 build_scalar_add_graph(KernelEntry *ke, u32 extent) {
  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
              ((u64)S_AXIS_LOOP << 32) | (u64)extent);
  u32 pa  = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pb  = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 1);
  u32 pc  = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 ia  = rangeify_emit_binary(ke, S_INDEX, DT_FP32, pa, r0);
  u32 ib  = rangeify_emit_binary(ke, S_INDEX, DT_FP32, pb, r0);
  u32 ic  = rangeify_emit_binary(ke, S_INDEX, DT_FP32, pc, r0);
  u32 la  = rangeify_emit_unary (ke, S_LOAD,  DT_FP32, ia);
  u32 lb  = rangeify_emit_unary (ke, S_LOAD,  DT_FP32, ib);
  u32 sum = rangeify_emit_binary(ke, S_ADD,   DT_FP32, la, lb);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, sum);
  u32 src[2] = {sto, r0};
  return rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, src, 0);
}

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

int main(void) {
  thvm_init();

  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("tile-graph/initial-state");
  CHECK_EQ((unsigned long long)ke->tile_uops, 0);
  CHECK_EQ(ke->n_tile_uops, 0);
  CHECK_EQ(ke->tile_uops_cap, 0);
  CHECK_EQ(ke->tile_root, 0);

  TEST_BEGIN("tile-graph/opname-helpers-cover-enum");
  for (u8 op = TILE_NONE; op < TILE__COUNT; op++) {
    const char *nm = tile_op_name(op);
    CHECK(nm != NULL);
    CHECK(nm[0] == 'T' && nm[1] == 'I');
  }
  CHECK_EQ((u64)tile_axis_name(KAX_LOOP)[0],   (u64)'L');
  CHECK_EQ((u64)tile_axis_name(KAX_REDUCE)[0], (u64)'R');
  CHECK_EQ((u64)tile_axis_name(KAX_UPCAST)[0], (u64)'U');

  TEST_BEGIN("tile-graph/build-from-scalar-ranges");
  u32 scalar_root = build_scalar_add_graph(ke, 8);
  u32 scalar_store = ke->scalar_uops[scalar_root].src[0];
  u32 scalar_value = ke->scalar_uops[scalar_store].src[1];
  CHECK(tile_build_from_scalar(ke));
  CHECK_EQ(ke->n_tile_uops, 5);       // sentinel + body + store + axis + loop
  CHECK_EQ(ke->tile_uops[0].op, TILE_NONE);
  CHECK_EQ(ke->tile_uops[1].op, TILE_SCALAR_BODY);
  CHECK_EQ((u32)ke->tile_uops[1].extra, scalar_value);
  CHECK_EQ(ke->tile_uops[2].op, TILE_STORE);
  CHECK_EQ((u32)ke->tile_uops[2].extra, scalar_store);
  CHECK_EQ(ke->tile_uops[2].src_count, 1);
  CHECK_EQ(ke->tile_uops[2].src[0], 1);
  CHECK_EQ(ke->tile_uops[3].op, TILE_AXIS);
  CHECK_EQ((u32)(ke->tile_uops[3].extra >> 32), (u32)KAX_LOOP);
  CHECK_EQ((u32)(ke->tile_uops[3].extra & 0xFFFFFFFFu), 8u);
  CHECK_EQ(ke->tile_uops[4].op, TILE_LOOP_NEST);
  CHECK_EQ(ke->tile_root, 4);
  CHECK_EQ(ke->tile_uops[4].src_count, 2);
  CHECK_EQ(ke->tile_uops[4].src[0], 2);
  CHECK_EQ(ke->tile_uops[4].src[1], 3);
  CHECK_EQ(ke->tile_uops[4].src[2], 0);
  CHECK(tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 1);
  CHECK_EQ(tile_loop_axis_type(ke, 0), (u32)KAX_LOOP);
  CHECK_EQ(tile_loop_axis_extent(ke, 0), 8u);

  TEST_BEGIN("tile-graph/kernel-axes-override");
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 2;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 2;
  ke->axes->axis_types[1] = KAX_UPCAST;
  ke->axes->full_shape[1] = 4;
  CHECK(tile_build_from_scalar(ke));
  CHECK_EQ(ke->n_tile_uops, 6);       // sentinel + body + store + two axes + loop
  CHECK_EQ(ke->tile_uops[2].op, TILE_STORE);
  CHECK_EQ(ke->tile_uops[3].op, TILE_AXIS);
  CHECK_EQ((u32)(ke->tile_uops[3].extra >> 32), (u32)KAX_LOOP);
  CHECK_EQ((u32)(ke->tile_uops[3].extra & 0xFFFFFFFFu), 2u);
  CHECK_EQ(ke->tile_uops[4].op, TILE_AXIS);
  CHECK_EQ((u32)(ke->tile_uops[4].extra >> 32), (u32)KAX_UPCAST);
  CHECK_EQ((u32)(ke->tile_uops[4].extra & 0xFFFFFFFFu), 4u);
  CHECK_EQ(ke->tile_uops[5].op, TILE_LOOP_NEST);
  CHECK_EQ(ke->tile_root, 5);
  CHECK_EQ(ke->tile_uops[5].src_count, 3);
  CHECK_EQ(ke->tile_uops[5].src[0], 2);
  CHECK_EQ(ke->tile_uops[5].src[1], 3);
  CHECK_EQ(ke->tile_uops[5].src[2], 4);
  CHECK(tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 2);
  CHECK_EQ(tile_loop_axis_type(ke, 0), (u32)KAX_LOOP);
  CHECK_EQ(tile_loop_axis_extent(ke, 0), 2u);
  CHECK_EQ(tile_loop_axis_type(ke, 1), (u32)KAX_UPCAST);
  CHECK_EQ(tile_loop_axis_extent(ke, 1), 4u);

  TEST_BEGIN("tile-graph/validator-rejects-bad-root");
  u32 good_root = ke->tile_root;
  ke->tile_root = 3;
  CHECK(!tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 0);
  ke->tile_root = good_root;
  CHECK(tile_validate(ke));
  u8 good_src_count = ke->tile_uops[good_root].src_count;
  ke->tile_uops[good_root].src_count = (u8)(TILE_MAX_SRC + 1);
  CHECK(!tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 0);
  ke->tile_uops[good_root].src_count = good_src_count;
  CHECK(tile_validate(ke));

  TEST_BEGIN("tile-graph/free-then-reemit");
  tile_free(ke);
  CHECK_EQ((unsigned long long)ke->tile_uops, 0);
  CHECK_EQ(ke->n_tile_uops, 0);
  CHECK_EQ(ke->tile_uops_cap, 0);
  CHECK_EQ(ke->tile_root, 0);
  for (u32 i = 0; i < 100; i++) {
    u32 id = tile_emit_leaf(ke, TILE_AXIS, DT_INT64,
                            ((u64)KAX_LOOP << 32) | i);
    CHECK_EQ(id, i + 1);
  }
  CHECK_EQ(ke->n_tile_uops, 101);
  CHECK(ke->tile_uops_cap >= 101);
  CHECK_EQ(ke->tile_uops[100].op, TILE_AXIS);
  CHECK_EQ((u32)ke->tile_uops[100].extra, 99u);
  CHECK_EQ(ke->tile_root, 0);
  CHECK(!tile_validate(ke));

  TEST_BEGIN("tile-graph/kernel-free-cleans-tile-and-scalar");
  kernel_free_arrays(ke);
  CHECK_EQ((unsigned long long)ke->tile_uops, 0);
  CHECK_EQ(ke->n_tile_uops, 0);
  CHECK_EQ(ke->tile_uops_cap, 0);
  CHECK_EQ(ke->tile_root, 0);
  CHECK_EQ((unsigned long long)ke->scalar_uops, 0);
  CHECK_EQ(ke->n_scalar_uops, 0);
  CHECK_EQ(ke->scalar_uops_cap, 0);

  TEST_BEGIN("tile-graph/materialize-auto-seeds-tile-plan");
  setenv("THVM_RANGEIFY", "1", 1);
  u32 dims[1] = {4};
  Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  Term b = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  thvm_materialize(uop_binary(UOP_ADD, a, b));
  KernelEntry *mk = &KERNELS[KERNELS_NEXT - 1];
  CHECK(mk->scalar_uops != NULL);
  CHECK(mk->n_scalar_uops > 1);
  CHECK(mk->tile_uops != NULL);
  CHECK(mk->n_tile_uops >= 5);
  CHECK_EQ(mk->tile_uops[0].op, TILE_NONE);
  CHECK_EQ(mk->tile_uops[1].op, TILE_SCALAR_BODY);
  CHECK_EQ(mk->tile_uops[2].op, TILE_STORE);
  CHECK(tile_validate(mk));
  CHECK(mk->tile_root != 0);
  CHECK_EQ(mk->tile_uops[mk->tile_root].op, TILE_LOOP_NEST);
  CHECK_EQ(tile_loop_axis_count(mk), 1);
  CHECK_EQ(tile_loop_axis_extent(mk, 0), 4u);

  thvm_free();
  TEST_REPORT();
}
