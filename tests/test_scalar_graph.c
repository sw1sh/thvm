// test_scalar_graph.c -- Phase A scaffolding test for the scalar-UOp
// arena owned by KernelEntry.  Builds a tiny scalar graph by hand
// (no rangeify yet -- that lands in Phase B) and asserts:
//   - slot 0 is the S_NONE sentinel and stays that way
//   - emit assigns monotonically increasing ids starting at 1
//   - reserve grows the arena geometrically and zeros the tail
//   - rangeify_free leaves a clean slate that can be re-emitted into
//   - opname / axisname helpers cover every enum value
//
// The graph we build mirrors the tinygrad shape for `c[i] = a[i] + b[i]`
// over a rank-1 N-element tensor:
//   r0       = S_RANGE  (extent N, axis_type LOOP)
//   pa       = S_DEFINE_PARAM(slot 0, dtype f32)
//   pb       = S_DEFINE_PARAM(slot 1, dtype f32)
//   pc       = S_DEFINE_OUTPUT(dtype f32)
//   ia       = S_INDEX(pa, r0)
//   ib       = S_INDEX(pb, r0)
//   ic       = S_INDEX(pc, r0)
//   la       = S_LOAD(ia)
//   lb       = S_LOAD(ib)
//   sum      = S_ADD(la, lb)
//   sto      = S_STORE(ic, sum)
//   buf      = S_BUFFERIZE(sto, r0)

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // Allocate a real KernelEntry to host the arena (so we exercise
  // the kernel_alloc -> rangeify_emit path and confirm
  // kernel_free_arrays cleans it up via rangeify_free).
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("scalar-graph/initial-state");
  CHECK_EQ((unsigned long long)ke->scalar_uops, 0);
  CHECK_EQ(ke->n_scalar_uops, 0);
  CHECK_EQ(ke->scalar_uops_cap, 0);

  TEST_BEGIN("scalar-graph/first-emit-reserves-sentinel");
  // First emit: should allocate scalar_uops, install S_NONE at slot 0,
  // and place the new op at slot 1.
  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
              ((u64)S_AXIS_LOOP << 32) | (u64)128u);
  CHECK_EQ(r0, 1);
  CHECK_EQ(ke->n_scalar_uops, 2);            // sentinel + r0
  CHECK(ke->scalar_uops_cap >= 2);
  CHECK_EQ(ke->scalar_uops[0].op, S_NONE);
  CHECK_EQ(ke->scalar_uops[0].src_count, 0);
  CHECK_EQ(ke->scalar_uops[1].op, S_RANGE);
  CHECK_EQ(ke->scalar_uops[1].src_count, 0);
  CHECK_EQ(ke->scalar_uops[1].dtype, DT_INT32);
  CHECK_EQ((u32)(ke->scalar_uops[1].extra & 0xFFFFFFFFu), 128u);
  CHECK_EQ((u32)(ke->scalar_uops[1].extra >> 32),         (u32)S_AXIS_LOOP);

  TEST_BEGIN("scalar-graph/build-add-graph-ids-monotone");
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
  // BUFFERIZE: 2 sources (body, range).
  u32 buf_src[2] = {sto, r0};
  u32 buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK_EQ(pa,  2);   CHECK_EQ(pb,  3);   CHECK_EQ(pc,  4);
  CHECK_EQ(ia,  5);   CHECK_EQ(ib,  6);   CHECK_EQ(ic,  7);
  CHECK_EQ(la,  8);   CHECK_EQ(lb,  9);   CHECK_EQ(sum, 10);
  CHECK_EQ(sto, 11);  CHECK_EQ(buf, 12);
  CHECK_EQ(ke->n_scalar_uops, 13);

  TEST_BEGIN("scalar-graph/structure-roundtrips");
  // Spot-check the structural fields land in scalar_uops[].
  CHECK_EQ(ke->scalar_uops[ia].op,        S_INDEX);
  CHECK_EQ(ke->scalar_uops[ia].src_count, 2);
  CHECK_EQ(ke->scalar_uops[ia].src[0],    pa);
  CHECK_EQ(ke->scalar_uops[ia].src[1],    r0);
  CHECK_EQ(ke->scalar_uops[ia].src[2],    0);  // unused, must be zero
  CHECK_EQ(ke->scalar_uops[buf].op,       S_BUFFERIZE);
  CHECK_EQ(ke->scalar_uops[buf].src_count,2);
  CHECK_EQ(ke->scalar_uops[buf].src[0],   sto);
  CHECK_EQ(ke->scalar_uops[buf].src[1],   r0);

  TEST_BEGIN("scalar-graph/free-then-reemit");
  // rangeify_free wipes pointer + counts; subsequent emit should
  // start fresh at slot 1 again.
  rangeify_free(ke);
  CHECK_EQ((unsigned long long)ke->scalar_uops, 0);
  CHECK_EQ(ke->n_scalar_uops, 0);
  CHECK_EQ(ke->scalar_uops_cap, 0);
  u32 fresh = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x40400000u); // 3.0f bits
  CHECK_EQ(fresh, 1);
  CHECK_EQ(ke->scalar_uops[0].op, S_NONE);
  CHECK_EQ(ke->scalar_uops[1].op, S_CONST);
  CHECK_EQ((u32)ke->scalar_uops[1].extra, 0x40400000u);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/opname-helpers-cover-enum");
  // Every enum value should have a non-"?" name.  Belt-and-braces
  // against future additions where someone forgets to extend the
  // switch in scalar_op_name.
  for (u8 op = S_NONE; op < S__COUNT; op++) {
    const char *nm = scalar_op_name(op);
    CHECK(nm != NULL);
    CHECK(nm[0] == 'S' && nm[1] == '_');
  }
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_LOOP)[0],   (u64)'L');
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_REDUCE)[0], (u64)'R');
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_UNROLL)[0], (u64)'U');
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_GLOBAL)[0], (u64)'G');

  TEST_BEGIN("scalar-graph/grow-many-ops");
  // Push enough emits to exercise multiple realloc rounds.
  // SUOP_INIT_CAP = 16, so 100 emits should hit at least 4 grows.
  for (u32 i = 0; i < 100; i++) {
    u32 id = rangeify_emit_leaf(ke, S_CONST, DT_FP32, i);
    CHECK_EQ(id, i + 1);
  }
  CHECK_EQ(ke->n_scalar_uops, 101);
  CHECK(ke->scalar_uops_cap >= 101);
  // Spot-check the tail wasn't trampled.
  CHECK_EQ((u32)ke->scalar_uops[100].extra, 99u);
  CHECK_EQ(ke->scalar_uops[100].op, S_CONST);
  rangeify_free(ke);

  thvm_free();
  TEST_REPORT();
}
