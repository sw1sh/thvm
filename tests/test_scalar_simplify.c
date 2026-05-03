// test_scalar_simplify.c -- smoke tests for the scalar-UOp simplification
// harness (src/scalar/simplify.c).  Phase 2 of the tinygrad rule port:
// the harness is wired but no real rules exist yet, so these tests cover
// only the driver itself: empty rule list is a no-op, a trivial fold
// rule fires once and remaps the root, and a graph that doesn't match
// any rule survives the pass unchanged.

#include "../src/thvm.c"
#include "test.h"

// Trivial rule: S_IADD(x, S_ICONST(0)) -> x.  Tests the apply +
// remap-chain machinery without exercising any real symbolic logic.
static u32 fold_iadd_zero(ScalarUop *uops, u32 n_uops, u32 node_id, void *user) {
  (void)user;
  if (node_id >= n_uops) return 0;
  ScalarUop const *u = &uops[node_id];
  if (u->op != S_IADD || u->src_count != 2) return 0;
  for (u8 i = 0; i < 2; i++) {
    u32 sid = u->src[i];
    if (sid == 0 || sid >= n_uops) continue;
    ScalarUop const *s = &uops[sid];
    if (s->op == S_ICONST && s->extra == 0) {
      // The other operand is the survivor.
      return u->src[i ^ 1];
    }
  }
  return 0;
}

// Build a tiny iter-arith graph and a BUFFERIZE root that wraps an
// S_IADD-of-zero so the trivial rule has something to fold.  Returns the
// id of the S_IADD node (the rewrite target).
static u32 build_iadd_zero_graph(KernelEntry *ke, u32 *out_other) {
  u32 r0    = rangeify_emit_leaf(ke, S_RANGE,  DT_INT32,
                                 ((u64)S_AXIS_LOOP << 32) | 4u);
  u32 zero  = rangeify_emit_leaf(ke, S_ICONST, DT_INT32, 0);
  u32 add   = rangeify_emit_binary(ke, S_IADD, DT_INT32, r0, zero);
  *out_other = r0;
  return add;
}

// Build a graph with no S_IADD nodes -- the trivial rule never fires,
// the harness must return the input root unchanged.
static u32 build_no_match_graph(KernelEntry *ke) {
  u32 r0   = rangeify_emit_leaf(ke, S_RANGE,  DT_INT32,
                                ((u64)S_AXIS_LOOP << 32) | 4u);
  u32 c1   = rangeify_emit_leaf(ke, S_ICONST, DT_INT32, 7);
  u32 prod = rangeify_emit_binary(ke, S_IMUL, DT_INT32, r0, c1);
  return prod;
}

int main(void) {
  thvm_init();

  // === empty rule list is a no-op ===
  TEST_BEGIN("scalar-simplify/empty-rules-is-noop");
  {
    KernelEntry ke = {0};
    u32 other;
    u32 add_id = build_iadd_zero_graph(&ke, &other);
    u32 saved_n = ke.n_scalar_uops;
    u32 saved_op = ke.scalar_uops[add_id].op;
    u32 saved_s0 = ke.scalar_uops[add_id].src[0];
    u32 saved_s1 = ke.scalar_uops[add_id].src[1];

    u32 out = scalar_simplify_apply(ke.scalar_uops, &ke.n_scalar_uops,
                                    add_id, NULL, 0, NULL);
    CHECK_EQ(out, add_id);
    CHECK_EQ(ke.n_scalar_uops, saved_n);
    CHECK_EQ(ke.scalar_uops[add_id].op, saved_op);
    CHECK_EQ(ke.scalar_uops[add_id].src[0], saved_s0);
    CHECK_EQ(ke.scalar_uops[add_id].src[1], saved_s1);
    rangeify_free(&ke);
  }

  // === trivial fold rule fires and remaps the root ===
  TEST_BEGIN("scalar-simplify/iadd-zero-folds");
  {
    KernelEntry ke = {0};
    u32 other;
    u32 add_id = build_iadd_zero_graph(&ke, &other);

    ScalarSimplifyRule rules[] = {{"fold-iadd-zero", fold_iadd_zero}};
    u32 out = scalar_simplify_apply(ke.scalar_uops, &ke.n_scalar_uops,
                                    add_id, rules, 1, NULL);
    CHECK(out != add_id);
    CHECK_EQ(out, other);
    CHECK_EQ(scalar_simplify_stat_hits("fold-iadd-zero"), 1);
    rangeify_free(&ke);
  }

  // === no-match graph survives the pass ===
  TEST_BEGIN("scalar-simplify/no-match-survives");
  {
    KernelEntry ke = {0};
    u32 root = build_no_match_graph(&ke);
    u32 saved_n = ke.n_scalar_uops;

    ScalarSimplifyRule rules[] = {{"fold-iadd-zero", fold_iadd_zero}};
    u32 out = scalar_simplify_apply(ke.scalar_uops, &ke.n_scalar_uops,
                                    root, rules, 1, NULL);
    CHECK_EQ(out, root);
    CHECK_EQ(ke.n_scalar_uops, saved_n);
    CHECK_EQ(scalar_simplify_stat_hits("fold-iadd-zero"), 0);
    rangeify_free(&ke);
  }

  TEST_REPORT();
}
