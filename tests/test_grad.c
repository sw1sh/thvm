// test_grad.c - chain-rule rewrite rule for UOP_GRAD.
//
// Step 13 minimal coverage: leaf cases, ADD, MUL, NEG, REDUCE_SUM,
// plus the structural wrapper that broadcasts the gradient to
// target's shape (ADD[raw, MUL[target, CONST(0)]]).  Numerical
// correctness is verified once the graph is materialized + dispatched
// (see WL test suite for the end-to-end f32 numerics).

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

// Strip the broadcast wrapper: interact_grad returns
//   ADD[raw_grad, MUL[target, CONST(0)]]
// so the actual chain-rule output sits in heap[loc + 0].
static Term unwrap(Term g) {
  if (term_tag(g) == TAG_UOP && term_ext(g) == UOP_ADD) {
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
  Term g = unwrap(wnf(uop_grad(a, gy, a)));
  CHECK_EQ(g, gy);

  TEST_BEGIN("grad/leaf-other-tensor-returns-zero");
  Term g0 = unwrap(wnf(uop_grad(b, gy, a)));
  CHECK_EQ(term_tag(g0), TAG_UOP);
  CHECK_EQ(term_ext(g0), UOP_CONST);

  TEST_BEGIN("grad/add-distributes");
  Term sum = uop_binary(UOP_ADD, a, b);          // y = a + b
  Term g_add = unwrap(wnf(uop_grad(sum, gy, a)));
  // Chain rule + leaf reductions: ADD[ gy, CONST(0) ].
  CHECK_EQ(term_tag(g_add), TAG_UOP);
  CHECK_EQ(term_ext(g_add), UOP_ADD);
  Term lhs = heap_read(term_val(g_add) + 0);
  Term rhs = heap_read(term_val(g_add) + 1);
  CHECK_EQ(lhs, gy);                              // ∂a/∂a · gy
  CHECK_EQ(term_ext(rhs), UOP_CONST);             // ∂b/∂a · gy = 0

  TEST_BEGIN("grad/mul-product-rule");
  Term mul = uop_binary(UOP_MUL, a, b);          // y = a * b
  Term g_mul = unwrap(wnf(uop_grad(mul, gy, a)));
  CHECK_EQ(term_ext(g_mul), UOP_ADD);
  Term mul_lhs = heap_read(term_val(g_mul) + 0); // GRAD[a, b*gy, a] = b*gy
  Term mul_rhs = heap_read(term_val(g_mul) + 1); // GRAD[b, a*gy, a] = 0
  CHECK_EQ(term_ext(mul_lhs), UOP_MUL);
  CHECK_EQ(term_ext(mul_rhs), UOP_CONST);

  TEST_BEGIN("grad/neg-pushes-into-cotangent");
  Term na = uop_unary(UOP_NEG, a);               // y = -a
  Term g_neg = unwrap(wnf(uop_grad(na, gy, a))); // d(-a)/da = -gy
  CHECK_EQ(term_ext(g_neg), UOP_NEG);

  TEST_BEGIN("grad/reduce-sum-pushes-cotangent");
  Term s = uop_reduce(REDUCE_SUM, 0, a);         // y = sum(a, axis=0)
  Term g_s = unwrap(wnf(uop_grad(s, gy, a)));    // d(sum)/da = gy
  CHECK_EQ(g_s, gy);

  TEST_BEGIN("grad/const-input-zero");
  Term cnst = uop_const(DT_F32, 0x40400000u);    // 3.0f bits
  Term g_c = unwrap(wnf(uop_grad(cnst, gy, a)));
  CHECK_EQ(term_ext(g_c), UOP_CONST);

  TEST_BEGIN("grad/wrapper-shape-broadcast-via-target");
  // Public interact_grad wraps every result in ADD[raw, MUL[target, CONST(0)]]
  // so materialize sees target's shape on the right side and produces a
  // tensor with target's shape rather than scalar gradients.
  Term g_wrap = wnf(uop_grad(a, gy, a));
  CHECK_EQ(term_ext(g_wrap), UOP_ADD);
  Term wrapped_rhs = heap_read(term_val(g_wrap) + 1);
  CHECK_EQ(term_ext(wrapped_rhs), UOP_MUL);
  CHECK_EQ(heap_read(term_val(wrapped_rhs) + 0), a);  // target * 0

  thvm_free();
  TEST_REPORT();
}
