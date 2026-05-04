// test_uop_movement_index.c - Phase B1: per-USE movement-chain resolver.
//
// Verifies `uop_resolve_movement_chain` peels movement ops outside-in
// and transforms the consumer's iter context.  Each new movement op
// landing in B1 gets its own block here.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // === UOP_PERMUTE ===
  TEST_BEGIN("movement-index/permute-iter-reorder");
  // Setup: PERMUTE(buf, 3, [2, 0, 1]) -- output axis 0 reads input axis 2,
  // output axis 1 reads input axis 0, output axis 2 reads input axis 1.
  Term buf = term_new(0, TAG_TEN, DT_FP32, 1);
  u32 perm[3] = {2, 0, 1};
  Term p = uop_permute(buf, 3, perm);
  // Build consumer's iters as RANGE leaves.
  Term out_iters[3] = {
    uop_range(0, S_AXIS_LOOP, 8),
    uop_range(1, S_AXIS_LOOP, 16),
    uop_range(2, S_AXIS_LOOP, 32),
  };
  Term resolved[3] = {out_iters[0], out_iters[1], out_iters[2]};
  u32 ndim = 3;
  Term bottom = uop_resolve_movement_chain(p, resolved, &ndim);
  // After PERMUTE perm=[2,0,1]: in_iters[2] = out_iters[0],
  // in_iters[0] = out_iters[1], in_iters[1] = out_iters[2].
  CHECK_EQ(bottom, buf);
  CHECK_EQ(ndim, 3);
  CHECK_EQ(resolved[0], out_iters[1]);
  CHECK_EQ(resolved[1], out_iters[2]);
  CHECK_EQ(resolved[2], out_iters[0]);

  TEST_BEGIN("movement-index/permute-identity-elided-by-constructor");
  // Identity permute is dropped by uop_permute itself, so the chain
  // bottoms out at the buffer immediately.
  u32 idperm[3] = {0, 1, 2};
  Term id_p = uop_permute(buf, 3, idperm);
  CHECK_EQ(id_p, buf);

  TEST_BEGIN("movement-index/permute-of-permute-composed-by-constructor");
  // PERMUTE(PERMUTE(buf, [2,0,1]), [1,2,0]) composes at construction
  // time.  Outer perm [1,2,0] means out[0]=inner[1], out[1]=inner[2],
  // out[2]=inner[0].  Inner perm [2,0,1] means inner[i] = src[perm[i]],
  // so the composed perm[i] = inner.perm[outer.perm[i]]:
  //   composed[0] = inner.perm[1] = 0
  //   composed[1] = inner.perm[2] = 1
  //   composed[2] = inner.perm[0] = 2
  // That's identity -> the chain collapses and uop_permute returns buf.
  u32 outer[3] = {1, 2, 0};
  Term composed = uop_permute(p, 3, outer);
  CHECK_EQ(composed, buf);

  TEST_BEGIN("movement-index/permute-non-identity-composition");
  // Non-identity outer composition.  Outer perm [0,2,1]:
  //   composed[0] = inner.perm[0] = 2
  //   composed[1] = inner.perm[2] = 1
  //   composed[2] = inner.perm[1] = 0
  // So the composed permute reorders [a,b,c] into [c,b,a].
  u32 outer2[3] = {0, 2, 1};
  Term composed2 = uop_permute(p, 3, outer2);
  CHECK_EQ(term_tag(composed2), TAG_UOP);
  CHECK_EQ(term_ext(composed2), UOP_PERMUTE);
  // Resolve through it: out_iters[0]=A, [1]=B, [2]=C ->
  //   in_iters[2]=A, in_iters[1]=B, in_iters[0]=C.
  Term iters2[3] = { out_iters[0], out_iters[1], out_iters[2] };
  u32 ndim2 = 3;
  Term bottom2 = uop_resolve_movement_chain(composed2, iters2, &ndim2);
  CHECK_EQ(bottom2, buf);
  CHECK_EQ(iters2[0], out_iters[2]);
  CHECK_EQ(iters2[1], out_iters[1]);
  CHECK_EQ(iters2[2], out_iters[0]);

  TEST_BEGIN("movement-index/non-movement-bottom-passes-through");
  // If src is not a movement op, the resolver returns it unchanged
  // and leaves iters alone.
  Term iters3[2] = { out_iters[0], out_iters[1] };
  u32 ndim3 = 2;
  Term r3 = uop_resolve_movement_chain(buf, iters3, &ndim3);
  CHECK_EQ(r3, buf);
  CHECK_EQ(iters3[0], out_iters[0]);
  CHECK_EQ(iters3[1], out_iters[1]);
  CHECK_EQ(ndim3, 2);

  TEST_BEGIN("movement-index/permute-rank-mismatch-bails");
  // If the consumer's ndim disagrees with PERMUTE's rank, bail (return 0).
  Term iters4[2] = { out_iters[0], out_iters[1] };
  u32 ndim4 = 2;
  Term r4 = uop_resolve_movement_chain(p, iters4, &ndim4);
  CHECK_EQ(r4, (Term)0);

  thvm_free();
  TEST_REPORT();
}
