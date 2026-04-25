// test_grad.c - chain-rule rewrite rule for UOP_GRAD.
//
// LAZY one-layer rewriting: each GRAD fire emits the immediate
// chain-rule form, deferring sub-positions as fresh UOP_GRAD nodes
// that fire when consumers reach them.  Tests inspect the
// outermost layer plus one extra fire for non-leaf cases.
//
// Numerical correctness end-to-end lives in wl/THVMLink/Tests/grad.wlt
// (TRealize drives the full materialize chain, firing every nested
// GRAD on the way down).

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

// Strip an outer UOP_EXPAND to reach what was lifted (target-shape
// wrap).  Used when the chain rule emits a leaf-broadcast.
static Term unexpand(Term g) {
  if (term_tag(g) == TAG_UOP && term_ext(g) == UOP_EXPAND) {
    return heap_read(term_val(g) + 0);
  }
  return g;
}

int main(void) {
  thvm_init();

  u32 d[1] = {3};
  u32 ta = alloc_f32_tensor(d, 1);
  u32 tb = alloc_f32_tensor(d, 1);
  Term a = term_new(0, TAG_TEN, DT_F32, ta);
  Term b = term_new(0, TAG_TEN, DT_F32, tb);
  Term gy = uop_const(DT_F32, 0x3f800000u);   // 1.0f bits

  TEST_BEGIN("grad/leaf-target-returns-gy");
  // y === target leaf: lifts gy to target.shape directly.
  Term g = wnf(uop_grad(a, gy, a));
  CHECK_EQ(term_ext(g), UOP_EXPAND);
  CHECK_EQ(unexpand(g), gy);

  TEST_BEGIN("grad/leaf-other-tensor-returns-zero");
  // Independent leaf: g = EXPAND(CONST(0), target.shape).
  Term g0 = wnf(uop_grad(b, gy, a));
  CHECK_EQ(term_ext(g0), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g0)), UOP_CONST);

  TEST_BEGIN("grad/add-distributes-lazily");
  // Lazy: one fire of grad(ADD[a,b], gy, a) produces
  //       ADD[ GRAD(a, gy, a), GRAD(b, gy, a) ]
  // -- the per-child GRADs stay un-fired until wnf reaches them.
  Term sum   = uop_binary(UOP_ADD, a, b);
  Term g_add = wnf(uop_grad(sum, gy, a));
  CHECK_EQ(term_ext(g_add), UOP_ADD);
  Term lhs = heap_read(term_val(g_add) + 0);
  Term rhs = heap_read(term_val(g_add) + 1);
  CHECK_EQ(term_ext(lhs), UOP_GRAD);
  CHECK_EQ(term_ext(rhs), UOP_GRAD);
  // Forcing each child fires the leaf rules.
  Term lhs_f = wnf(lhs);
  Term rhs_f = wnf(rhs);
  CHECK_EQ(unexpand(lhs_f), gy);                  // ∂a/∂a · gy
  CHECK_EQ(term_ext(unexpand(rhs_f)), UOP_CONST); // ∂b/∂a · gy = 0

  TEST_BEGIN("grad/mul-product-rule-lazily");
  // grad(MUL[a,b], gy, a) -> ADD[ GRAD(a, MUL[b, EXPAND(gy)], a),
  //                                GRAD(b, MUL[a, EXPAND(gy)], a) ]
  Term mul   = uop_binary(UOP_MUL, a, b);
  Term g_mul = wnf(uop_grad(mul, gy, a));
  CHECK_EQ(term_ext(g_mul), UOP_ADD);
  Term mlhs = heap_read(term_val(g_mul) + 0);
  Term mrhs = heap_read(term_val(g_mul) + 1);
  CHECK_EQ(term_ext(mlhs), UOP_GRAD);
  CHECK_EQ(term_ext(mrhs), UOP_GRAD);
  // mlhs's gy slot is the per-branch MUL[b, EXPAND(gy)].
  Term mlhs_gy = heap_read(term_val(mlhs) + 1);
  CHECK_EQ(term_ext(mlhs_gy), UOP_MUL);
  // Forcing mlhs fires its leaf branch (a == target -> EXPAND(MUL...)).
  Term mlhs_f = wnf(mlhs);
  CHECK_EQ(term_ext(mlhs_f), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(mlhs_f)), UOP_MUL);

  TEST_BEGIN("grad/neg-cascades-through-leaf");
  // Single-child rewrites (NEG / REDUCE) leave a UOP_GRAD at the
  // root after one fire; wnf immediately re-enters that GRAD on
  // the same pass, so the user-visible result is the cascaded
  // leaf-match form: EXPAND(NEG[EXPAND(gy)]).  (The "lazy" win
  // appears in multi-branch ops like ADD / MUL where the top
  // wrapper isn't a UOP_GRAD, so wnf doesn't keep firing.)
  Term na    = uop_unary(UOP_NEG, a);
  Term g_neg = wnf(uop_grad(na, gy, a));
  CHECK_EQ(term_ext(g_neg), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_neg)), UOP_NEG);

  TEST_BEGIN("grad/reduce-sum-cascades-through-leaf");
  Term s   = uop_reduce(REDUCE_SUM, 0, a);
  Term g_s = wnf(uop_grad(s, gy, a));
  // Cascades to leaf-match: EXPAND(EXPAND(gy)) -- outer from leaf,
  // inner from the REDUCE-rule's gy lift.  Both reach back to gy.
  CHECK_EQ(term_ext(g_s), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_s)), UOP_EXPAND);

  TEST_BEGIN("grad/const-input-zero");
  // CONST never depends on target, so this is a one-shot zero.
  Term cnst = uop_const(DT_F32, 0x40400000u);    // 3.0f bits
  Term g_c  = wnf(uop_grad(cnst, gy, a));
  CHECK_EQ(term_ext(g_c), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_c)), UOP_CONST);

  TEST_BEGIN("grad/upfront-expand-carries-target-shape");
  // The leaf-target rule lifts gy to target.shape (via EXPAND with
  // dims read from TENS).
  Term g_lift = wnf(uop_grad(a, gy, a));
  CHECK_EQ(term_ext(g_lift), UOP_EXPAND);
  Term dim0 = heap_read(term_val(g_lift) + 1);
  CHECK_EQ(term_tag(dim0), TAG_NUM);
  CHECK_EQ(term_val(dim0), 3);

  thvm_free();
  TEST_REPORT();
}
