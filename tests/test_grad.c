// test_grad.c - chain-rule rewrite rule for UOP_GRAD.
//
// Step 13 minimal coverage: leaf cases, ADD, MUL, NEG, REDUCE_SUM.
// `interact_grad` lifts gy to target.shape ONCE upfront via EXPAND;
// the chain rule below it then operates on target-shaped tensors,
// so leaf emissions return the lifted gy directly (no per-leaf
// EXPAND wrapping).  Numerical correctness lives in the WL test
// suite (wl/THVMLink/Tests/grad.wlt).

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

// Strip the upfront EXPAND wrapping for structural checks where we
// want to reach the original CONST/MUL/etc. inside.
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
  // Leaf match: g = EXPAND(gy, target.shape) (the upfront lift).
  Term g = wnf(uop_grad(a, gy, a));
  CHECK_EQ(term_ext(g), UOP_EXPAND);
  CHECK_EQ(unexpand(g), gy);

  TEST_BEGIN("grad/leaf-other-tensor-returns-zero");
  // Independent leaf: g = EXPAND(CONST(0), target.shape).
  Term g0 = wnf(uop_grad(b, gy, a));
  CHECK_EQ(term_ext(g0), UOP_EXPAND);
  Term g0_inner = unexpand(g0);
  CHECK_EQ(term_ext(g0_inner), UOP_CONST);

  TEST_BEGIN("grad/add-distributes");
  Term sum = uop_binary(UOP_ADD, a, b);          // y = a + b
  Term g_add = wnf(uop_grad(sum, gy, a));
  // Chain rule: ADD[ EXPAND(gy), EXPAND(CONST(0)) ].
  CHECK_EQ(term_ext(g_add), UOP_ADD);
  Term lhs = heap_read(term_val(g_add) + 0);
  Term rhs = heap_read(term_val(g_add) + 1);
  CHECK_EQ(unexpand(lhs), gy);                    // ∂a/∂a · gy
  CHECK_EQ(term_ext(unexpand(rhs)), UOP_CONST);   // ∂b/∂a · gy = 0

  TEST_BEGIN("grad/mul-product-rule");
  Term mul = uop_binary(UOP_MUL, a, b);          // y = a * b
  Term g_mul = wnf(uop_grad(mul, gy, a));
  CHECK_EQ(term_ext(g_mul), UOP_ADD);
  Term mul_lhs = heap_read(term_val(g_mul) + 0); // MUL[b, EXPAND(gy)] -- target-shaped both sides
  Term mul_rhs = heap_read(term_val(g_mul) + 1); // EXPAND(CONST(0))
  CHECK_EQ(term_ext(mul_lhs), UOP_MUL);
  CHECK_EQ(term_ext(unexpand(mul_rhs)), UOP_CONST);

  TEST_BEGIN("grad/neg-pushes-into-cotangent");
  Term na = uop_unary(UOP_NEG, a);               // y = -a
  Term g_neg = wnf(uop_grad(na, gy, a));
  // d(-a)/da: leaf match returns NEG[lifted_gy] (no extra EXPAND --
  // NEG of a target-shaped value is target-shaped).
  CHECK_EQ(term_ext(g_neg), UOP_NEG);

  TEST_BEGIN("grad/reduce-sum-pushes-cotangent");
  Term s = uop_reduce(REDUCE_SUM, 0, a);         // y = sum(a, axis=0)
  Term g_s = wnf(uop_grad(s, gy, a));            // d(sum)/da = EXPAND(gy)
  CHECK_EQ(term_ext(g_s), UOP_EXPAND);
  CHECK_EQ(unexpand(g_s), gy);

  TEST_BEGIN("grad/const-input-zero");
  Term cnst = uop_const(DT_F32, 0x40400000u);    // 3.0f bits
  Term g_c = wnf(uop_grad(cnst, gy, a));
  CHECK_EQ(term_ext(g_c), UOP_EXPAND);
  CHECK_EQ(term_ext(unexpand(g_c)), UOP_CONST);

  TEST_BEGIN("grad/upfront-expand-carries-target-shape");
  // The upfront EXPAND in interact_grad wraps gy with target's shape
  // (read from TENS) so chain-rule intermediates stay target-shaped.
  Term g_lift = wnf(uop_grad(a, gy, a));
  CHECK_EQ(term_ext(g_lift), UOP_EXPAND);
  Term dim0 = heap_read(term_val(g_lift) + 1);
  CHECK_EQ(term_tag(dim0), TAG_NUM);
  CHECK_EQ(term_val(dim0), 3);

  thvm_free();
  TEST_REPORT();
}
