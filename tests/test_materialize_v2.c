// test_materialize_v2.c - g2a coverage: scheduler skeleton +
// topo-sort over realize boundaries.
//
// At this stage thvm_materialize doesn't emit kernels; it only
// runs realize_classify and topo-sorts the boundary set.  Tests
// inspect that order via materialize_boundary_count() /
// materialize_boundary_at().

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static Term kernel_id_from_term(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_KERNEL) return 0;
  return heap_read(term_val(t) + 1);
}

int main(void) {
  setenv("THVM_UOP_GRAPH_SIMPLIFY", "0", 1);
  thvm_init();

  // Shared-subexpression case: s = a + b appears as the source of
  // TWO distinct parents (left = MUL[s, c], right = MUL[s, d]); the
  // sink ADD merges them.  realize_classify rules:
  //   - sink is the root             -> realized
  //   - left, right consumers=1     -> not realized
  //   - s consumers=2 (multi)       -> realized
  // Expected: BOUNDARY_ORDER = [s, sink], depth(s)=1, depth(sink)=2.
  TEST_BEGIN("materialize-v2/shared-subexpr-yields-two-boundaries");
  u32 ta = alloc_f32_tensor(4), tb = alloc_f32_tensor(4);
  u32 tc = alloc_f32_tensor(4), td = alloc_f32_tensor(4);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term c = term_new(0, TAG_TEN, DT_FP32, tc);
  Term d = term_new(0, TAG_TEN, DT_FP32, td);
  Term s     = uop_binary(UOP_ADD, a, b);
  Term left  = uop_binary(UOP_MUL, s, c);
  Term right = uop_binary(UOP_MUL, s, d);
  Term sink  = uop_binary(UOP_ADD, left, right);
  thvm_materialize(sink);
  CHECK_EQ(materialize_boundary_count(), 2u);
  CHECK_EQ(materialize_boundary_at(0), term_val(s));
  CHECK_EQ(materialize_boundary_at(1), term_val(sink));

  // Linear elementwise chain: each intermediate has consumers=1 and
  // is not REDUCE, so only the sink is a boundary.
  TEST_BEGIN("materialize-v2/linear-chain-yields-one-boundary");
  Term ab    = uop_binary(UOP_ADD, a, b);
  Term abc   = uop_binary(UOP_ADD, ab, c);
  Term sink2 = uop_binary(UOP_ADD, abc, d);
  thvm_materialize(sink2);
  CHECK_EQ(materialize_boundary_count(), 1u);
  CHECK_EQ(materialize_boundary_at(0), term_val(sink2));

  // REDUCE always realizes (rule c), even in the middle of a chain.
  // sum(a + b) -- the REDUCE is both root AND a REDUCE; the inner
  // ADD has consumers=1 and is not realized.
  TEST_BEGIN("materialize-v2/reduce-as-sole-boundary");
  Term sum_inner = uop_binary(UOP_ADD, a, b);
  Term reduced   = uop_reduce(REDUCE_SUM, 0, sum_inner);
  thvm_materialize(reduced);
  CHECK_EQ(materialize_boundary_count(), 1u);
  CHECK_EQ(materialize_boundary_at(0), term_val(reduced));

  // Chain of two REDUCEs: each is a separate boundary; topo order
  // puts the upstream one first.
  TEST_BEGIN("materialize-v2/reduce-chain-yields-two-boundaries");
  Term inner_red = uop_reduce(REDUCE_SUM, 0, sum_inner);
  Term outer_red = uop_reduce(REDUCE_SUM, 0, inner_red);
  thvm_materialize(outer_red);
  CHECK_EQ(materialize_boundary_count(), 2u);
  CHECK_EQ(materialize_boundary_at(0), term_val(inner_red));
  CHECK_EQ(materialize_boundary_at(1), term_val(outer_red));

  TEST_BEGIN("materialize-v2/repeated-inline-uop-lowers-once");
  thvm_free();
  thvm_init();
  u32 te0 = alloc_f32_tensor(4), te1 = alloc_f32_tensor(4);
  u32 te2 = alloc_f32_tensor(4), te3 = alloc_f32_tensor(4);
  Term e0 = term_new(0, TAG_TEN, DT_FP32, te0);
  Term e1 = term_new(0, TAG_TEN, DT_FP32, te1);
  Term e2 = term_new(0, TAG_TEN, DT_FP32, te2);
  Term e3 = term_new(0, TAG_TEN, DT_FP32, te3);
  Term sum01 = uop_binary(UOP_ADD, e0, e1);
  Term sum23 = uop_binary(UOP_ADD, e2, e3);
  Term tree = uop_binary(UOP_ADD, sum01, sum23);
  Term square = uop_binary(UOP_MUL, tree, tree);
  Term out = thvm_materialize(square);
  Term kid_term = kernel_id_from_term(out);
  CHECK_EQ(term_tag(kid_term), TAG_NUM);
  u32 kid = (u32)term_val(kid_term);
  CHECK_EQ(KERNELS[kid].n_inputs, 4u);
  // Phase C slice 7: program[] is freed post-lift by default
  // (THVM_PHASE_C7_FREE_PROGRAM=0 reverts).  When the program is
  // freed, the legacy n_ops/program-row-shape checks no longer
  // apply -- the canonical kernel representation is the lifted
  // UOp DAG (KERNELS[kid].compute_root); kernel_lift coverage
  // tests + test_compute_root_dual_write validate the equivalent.
  char const *m_free_e = getenv("THVM_PHASE_C7_FREE_PROGRAM");
  int m_free_on = (m_free_e != NULL) && (m_free_e[0] == '1');
  if (!m_free_on) {
    CHECK_EQ(KERNELS[kid].n_ops, 4u);
    CHECK_EQ(KERNELS[kid].program[3].opcode, UOP_MUL);
    CHECK_EQ(KERNELS[kid].program[3].src[0], KERNELS[kid].program[3].src[1]);
  } else {
    CHECK_EQ(KERNELS[kid].n_ops, 0u);
    CHECK(KERNELS[kid].compute_root != 0);
  }

  thvm_free();
  TEST_REPORT();
}
