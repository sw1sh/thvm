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

  TEST_BEGIN("grad/reduce-max-cascades-through-mask-mul");
  // GRAD[REDUCE_MAX(a, 0), gy, a] -> GRAD[a, MUL[lift(gy), mask], a]
  // -> leaf-EXPAND wrapping a MUL.  Mask = CMPEQ(a, EXPAND(MAX(a))).
  Term mxr  = uop_reduce(REDUCE_MAX, 0, a);
  Term g_mx = wnf(uop_grad(mxr, gy, a));
  CHECK_EQ(term_ext(g_mx), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_mx)), UOP_MUL);

  TEST_BEGIN("grad/reshape-passthrough-cascades-to-leaf");
  // RESHAPE is identity-on-data so the rule is a pure passthrough:
  // GRAD[RESHAPE(a, ...), gy, a] -> GRAD[a, gy, a].  That nested GRAD
  // immediately cascades through the leaf rule (a === target) on the
  // same wnf pass, so the user-visible form is the leaf-EXPAND.
  u32 new_shape[2] = {3, 1};
  Term r     = uop_reshape(a, 2, new_shape);
  Term g_r   = wnf(uop_grad(r, gy, a));
  CHECK_EQ(term_ext(g_r), UOP_EXPAND);
  CHECK_EQ(unexpand(g_r), gy);

  TEST_BEGIN("grad/log2-emits-mul-with-recip-and-inv-ln2");
  // GRAD[LOG2(a), gy, a] -> GRAD[a, gy * RECIP(a) * CONST(1/ln2), a]
  // -> cascade to leaf-EXPAND wrapping a MUL.
  Term la     = uop_unary(UOP_LOG2, a);
  Term g_lg   = wnf(uop_grad(la, gy, a));
  CHECK_EQ(term_ext(g_lg), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_lg)), UOP_MUL);

  TEST_BEGIN("grad/exp2-emits-mul-with-exp2-and-ln2");
  // GRAD[EXP2(a), gy, a] -> GRAD[a, gy * EXP2(a) * CONST(ln2), a]
  // -> cascade to leaf-EXPAND wrapping a MUL.
  Term ea     = uop_unary(UOP_EXP2, a);
  Term g_ex   = wnf(uop_grad(ea, gy, a));
  CHECK_EQ(term_ext(g_ex), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_ex)), UOP_MUL);

  TEST_BEGIN("grad/recip-emits-mul-neg-recip-squared");
  // GRAD[RECIP(a), gy, a] -> GRAD[a, gy * -(1/a)*(1/a), a].
  // After one fire, the chain rule produces a MUL[gy, NEG[MUL[RECIP,RECIP]]]
  // wrapped in the GRAD on a; that GRAD then cascades to leaf-EXPAND.
  Term ra     = uop_unary(UOP_RECIP, a);
  Term g_rec  = wnf(uop_grad(ra, gy, a));
  // After the recip rule + leaf cascade: EXPAND wrapping a MUL chain.
  CHECK_EQ(term_ext(g_rec), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_rec)), UOP_MUL);

  TEST_BEGIN("grad/expand-of-const-is-zero");
  // EXPAND of a CONST short-circuits: constants have no gradient
  // wrt anything.  Output is target-shaped grad_zero.
  u32 dim3[1]  = {3};
  Term cst_e   = uop_const(DT_F32, 0x3f800000u);
  Term ex_c    = uop_expand(cst_e, 1, dim3);
  Term g_ec    = wnf(uop_grad(ex_c, gy, a));
  CHECK_EQ(term_ext(g_ec), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_ec)), UOP_CONST);

  TEST_BEGIN("grad/expand-from-shape-1-reduces-along-axis");
  // EXPAND of a TEN with shape {1} to shape {3} should emit a
  // REDUCE_SUM along axis 0 in the cotangent path.
  u32 d1[1]   = {1};
  u32 to3_[1] = {3};
  u32 t1      = alloc_f32_tensor(d1, 1);
  Term a1     = term_new(0, TAG_TEN, DT_F32, t1);
  Term ex1    = uop_expand(a1, 1, to3_);
  Term g_e    = wnf(uop_grad(ex1, gy, a1));
  // Cascades to leaf-EXPAND wrapping the reduced cotangent (a REDUCE).
  CHECK_EQ(term_ext(g_e), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_e)), UOP_REDUCE);

  TEST_BEGIN("grad/cmplt-is-non-differentiable-zero");
  // CMPLT mask is treated as a constant: GRAD[CMPLT(a,b), gy, t] -> 0.
  Term cmp     = uop_binary(UOP_CMPLT, a, b);
  Term g_cmp   = wnf(uop_grad(cmp, gy, a));
  CHECK_EQ(term_ext(g_cmp), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_cmp)), UOP_CONST);

  TEST_BEGIN("grad/relu-pattern-grad-equals-mask");
  // ReLU = MUL[x, CMPLT(0, x)].  d(ReLU)/dx = mask, structurally:
  // MUL rule splits to ADD[GRAD(x, mask*gy, x), GRAD(mask, x*gy, x)].
  // The second branch hits the CMPLT-zero rule above.
  Term zero    = uop_const(DT_F32, 0);
  Term mask    = uop_binary(UOP_CMPLT, zero, a);
  Term relu    = uop_binary(UOP_MUL, a, mask);
  Term g_relu  = wnf(uop_grad(relu, gy, a));
  CHECK_EQ(term_ext(g_relu), UOP_ADD);

  TEST_BEGIN("grad/shrink-emits-pad-on-cotangent");
  // GRAD[SHRINK(a, [{1,4}]), gy, a] -> GRAD[a, PAD(gy, [1,1]), a]
  // -> cascade to leaf: EXPAND(PAD(gy, [1,1]), [5]).  Source is
  // length 5; SHRINK keeps indices [1,4) -> length 3; the grad
  // PADs gy with 1 zero on each side.
  u32 d5[1] = {5};
  u32 t5    = alloc_f32_tensor(d5, 1);
  Term a5   = term_new(0, TAG_TEN, DT_F32, t5);
  u32 sh_be[2] = {1, 4};
  Term sk   = uop_shrink(a5, 1, sh_be);
  Term g_sk = wnf(uop_grad(sk, gy, a5));
  CHECK_EQ(term_ext(g_sk), UOP_EXPAND);
  Term inside = unexpand(g_sk);
  CHECK_EQ(term_ext(inside), UOP_PAD);
  // PAD heap: [src, NUM(b0)=1, NUM(e0)=1].
  Term b_cell = heap_read(term_val(inside) + 1);
  Term e_cell = heap_read(term_val(inside) + 2);
  CHECK_EQ(term_val(b_cell), 1);
  CHECK_EQ(term_val(e_cell), 1);

  TEST_BEGIN("grad/flip-emits-flip-on-cotangent");
  // FLIP is its own inverse, so GRAD[FLIP(a, mask), gy, a] emits
  // GRAD[a, FLIP(gy, mask), a] -> cascade to leaf:
  //   EXPAND(FLIP(gy, mask), a.shape).
  // Use mask = 1 (flip axis 0); inner FLIP heap holds NUM(1).
  Term fl     = uop_flip(a, 1u);
  Term g_fl   = wnf(uop_grad(fl, gy, a));
  CHECK_EQ(term_ext(g_fl), UOP_EXPAND);
  Term inner_fl = unexpand(g_fl);
  CHECK_EQ(term_ext(inner_fl), UOP_FLIP);
  Term mask_cell = heap_read(term_val(inner_fl) + 1);
  CHECK_EQ(term_val(mask_cell), 1);

  TEST_BEGIN("grad/permute-emits-inverse-permute-on-cotangent");
  // 2-D source {2, 3} -> PERMUTE(perm = {1, 0}) -> {3, 2}.  The
  // inverse permutation is also {1, 0} (involution at rank 2), so
  // GRAD emits PERMUTE(gy_lifted, {1, 0}) and the leaf rule wraps
  // it in EXPAND back to a's shape.
  u32 d2x3[2] = {2, 3};
  u32 t2x3    = alloc_f32_tensor(d2x3, 2);
  Term a23    = term_new(0, TAG_TEN, DT_F32, t2x3);
  u32 perm10[2] = {1, 0};
  Term pm     = uop_permute(a23, 2, perm10);
  Term g_pm   = wnf(uop_grad(pm, gy, a23));
  CHECK_EQ(term_ext(g_pm), UOP_EXPAND);
  Term inner_pm = unexpand(g_pm);
  CHECK_EQ(term_ext(inner_pm), UOP_PERMUTE);
  // PERMUTE heap: [src, NUM(0)=inv_perm[0], NUM(1)=inv_perm[1]] =
  // [src, NUM(1), NUM(0)] for perm = {1, 0}.
  Term ip0 = heap_read(term_val(inner_pm) + 1);
  Term ip1 = heap_read(term_val(inner_pm) + 2);
  CHECK_EQ(term_val(ip0), 1);
  CHECK_EQ(term_val(ip1), 0);

  TEST_BEGIN("grad/pad-emits-shrink-on-cotangent");
  // GRAD[PAD(a, [{1,1}]), gy, a] -> GRAD[a, SHRINK(gy, [{1, 1+3})), a]
  // -> cascade to leaf: EXPAND(SHRINK(gy, [1, 4)), [3]).  Source is
  // length 3; PAD widens to length 5; the grad SHRINKs the cotangent
  // back to [1, 4) -- the inner source slice.
  u32 pd_be[2] = {1, 1};
  Term pd     = uop_pad(a, 1, pd_be);
  Term g_pd   = wnf(uop_grad(pd, gy, a));
  CHECK_EQ(term_ext(g_pd), UOP_EXPAND);
  Term inner_pd = unexpand(g_pd);
  CHECK_EQ(term_ext(inner_pd), UOP_SHRINK);
  // SHRINK heap: [src, NUM(b0)=1, NUM(e0)=4].  Begin = pad-begin,
  // end = pad-begin + src_dim = 1 + 3 = 4.
  Term sb = heap_read(term_val(inner_pd) + 1);
  Term se = heap_read(term_val(inner_pd) + 2);
  CHECK_EQ(term_val(sb), 1);
  CHECK_EQ(term_val(se), 4);

  TEST_BEGIN("grad/upfront-expand-carries-target-shape");
  // The leaf-target rule lifts gy to target.shape (via EXPAND with
  // dims read from TENS).  Heap layout is [src, NUM(ndim), NUM(d0), ...]
  // so the first dim cell sits at offset +2.
  Term g_lift = wnf(uop_grad(a, gy, a));
  CHECK_EQ(term_ext(g_lift), UOP_EXPAND);
  Term ndimN  = heap_read(term_val(g_lift) + 1);
  CHECK_EQ(term_tag(ndimN), TAG_NUM);
  CHECK_EQ(term_val(ndimN), 1);
  Term dim0   = heap_read(term_val(g_lift) + 2);
  CHECK_EQ(term_tag(dim0), TAG_NUM);
  CHECK_EQ(term_val(dim0), 3);

  TEST_BEGIN("grad/multi-target-unary-wrapper-uses-new-layout");
  Term g_unary = uop_grad(a, gy, a);
  CHECK_EQ(uop_grad_n(g_unary),     1u);
  CHECK_EQ(uop_grad_target(g_unary, 0), a);
  CHECK_EQ(uop_grad_target(g_unary, 1), 0u);
  u64  g_unary_loc   = term_val(g_unary);
  Term g_unary_n_cell = heap_read(g_unary_loc + 2);
  CHECK_EQ(term_tag(g_unary_n_cell), TAG_NUM);
  CHECK_EQ(term_val(g_unary_n_cell), 1u);
  CHECK_EQ(heap_read(g_unary_loc + 3), a);

  TEST_BEGIN("grad/multi-target-three-targets-heap-layout");
  u32  d3[1] = {3};
  u32  tc    = alloc_f32_tensor(d3, 1);
  Term c     = term_new(0, TAG_TEN, DT_F32, tc);
  Term targets[3] = {a, b, c};
  Term g_multi = uop_grad_multi(a, gy, targets, 3);
  CHECK_EQ(term_tag(g_multi), TAG_UOP);
  CHECK_EQ(term_ext(g_multi), UOP_GRAD);
  CHECK_EQ(uop_grad_n(g_multi),     3u);
  CHECK_EQ(uop_grad_target(g_multi, 0), a);
  CHECK_EQ(uop_grad_target(g_multi, 1), b);
  CHECK_EQ(uop_grad_target(g_multi, 2), c);
  CHECK_EQ(uop_grad_target(g_multi, 3), 0u);

  TEST_BEGIN("grad/multi-target-interact-emits-ctr-of-n-grads");
  Term g_multi_after = interact_grad(g_multi);
  CHECK_EQ(term_tag(g_multi_after), TAG_CTR);
  CHECK_EQ(term_ctr_n(g_multi_after),     3u);
  for (u32 i = 0; i < 3; i++) {
    Term g_i = term_ctr_at(g_multi_after, i);
    CHECK_EQ(term_tag(g_i), TAG_UOP);
    CHECK_EQ(term_ext(g_i), UOP_GRAD);
    CHECK_EQ(uop_grad_n(g_i), 1u);
    CHECK_EQ(uop_grad_target(g_i, 0), targets[i]);
    u64 g_i_loc = term_val(g_i);
    CHECK_EQ(heap_read(g_i_loc + 0), a);
  }

  TEST_BEGIN("grad/multi-target-shared-y-loc-across-grads");
  Term mul_shared = uop_binary(UOP_MUL, a, b);
  Term g_mul_multi = uop_grad_multi(mul_shared, gy, targets, 3);
  Term g_mul_after = interact_grad(g_mul_multi);
  CHECK_EQ(term_tag(g_mul_after), TAG_CTR);
  CHECK_EQ(term_ctr_n(g_mul_after),     3u);
  u64 first_y_loc = term_val(heap_read(term_val(term_ctr_at(g_mul_after, 0)) + 0));
  for (u32 i = 1; i < 3; i++) {
    u64 ith_y_loc = term_val(heap_read(term_val(term_ctr_at(g_mul_after, i)) + 0));
    CHECK_EQ(ith_y_loc, first_y_loc);
  }

  thvm_free();
  TEST_REPORT();
}
