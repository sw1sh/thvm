// test_uop_symbolic.c - exercises src/uop/symbolic_rewrite.c, the port
// of (a subset of) tinygrad codegen/uop/symbolic.py "sym" passes to
// thvm's UOp graph rewrite framework.
//
// The constructor-level rewrites in uop/rewrite.c + uop/index_simplify.c
// already cover most identity cases at build time, but they only fire
// when the simplified-shape outputs are constructed via uop_*().  The
// graph-rewrite sym pass is needed because:
//
//   1. Expander + devectorizer produce STACK / GEP / UNROLL wrappers
//      around already-constructed nodes; their internal builders don't
//      re-trigger float / mixed identities that only become visible
//      after lane decomposition (e.g. STACK(MUL(x_i, CONST(0))) lanes
//      that should each collapse to CONST(0)).
//
//   2. Devectorization can rebuild ALU around new constant operands
//      (CONST(0) acc-init from PLACEHOLDER, GEP-extracted VCONST
//      element bits) WITHOUT routing through the constructor (the
//      devectorizer assembles STACK + ALU around hash-consed children).
//      A graph rewrite that re-invokes the simplifying constructor
//      catches them.
//
// Coverage rules (each verified separately):
//   sym1  : ADD(x, CONST(0.0)) -> x  + commute
//   sym2  : MUL(x, CONST(1.0)) -> x  + commute
//   sym3  : MUL(x, CONST(0.0)) -> CONST(0.0)  + commute   (SCALAR ONLY)
//   sym4  : ADD(CONST(a), CONST(b)) -> CONST(a+b)  (constant fold)
//   sym5  : MUL(CONST(a), CONST(b)) -> CONST(a*b)
//   sym6  : NEG(NEG(x)) -> x
//   sym7  : RECIP(RECIP(x)) -> x
//   sym8  : Integer: IADD(x, CONST(0)) -> x
//   sym9  : Integer: IMUL(x, CONST(1)) -> x
//   sym10 : Integer: IMUL(x, CONST(0)) -> CONST(0)
//   sym11 : Integer: nested constant fold (IADD/IMUL of consts)
//   sym12 : GEP(STACK(s0, s1, s2, ..., sk), (i,)) -> s_i (singleton arg)
//   sym13 : Idempotent rewrite (running sym twice changes nothing)
//   sym14 : Deep recursive: walks under STACK to fold child MUL(x, CONST(0))
//   sym15 : No-op preserves identity (when nothing to fold)

#include "../src/thvm.c"
#include "test.h"

static u32 f32_to_bits_test(float v) {
  u32 b; memcpy(&b, &v, sizeof b); return b;
}

static int is_const_zero_f32(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term cell = heap_read(term_val(t));
  if (term_tag(cell) != TAG_NUM) return 0;
  u32 bits = (u32)term_val(cell);
  f32 v; memcpy(&v, &bits, sizeof v);
  return v == 0.0f;
}

static int is_const_with_bits(Term t, u32 want_bits) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term cell = heap_read(term_val(t));
  if (term_tag(cell) != TAG_NUM) return 0;
  return (u32)term_val(cell) == want_bits;
}

// Build an opaque non-CONST f32 leaf so the rewrites don't fold it
// away at construction time.  An INDEX_E LOAD is the cleanest source.
static Term mk_opaque_f32(void) {
  u32 dims[1] = { 8 };
  Term buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term addr = uop_const(DT_INT32, 0);
  Term idx = uop_index_e(buf, addr);
  return uop_load(idx);
}

static Term mk_opaque_f32_b(void) {
  u32 dims[1] = { 8 };
  Term buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 2);
  Term addr = uop_const(DT_INT32, 0);
  Term idx = uop_index_e(buf, addr);
  return uop_load(idx);
}

static Term mk_opaque_i32(void) {
  u32 dims[1] = { 8 };
  Term buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_INT32, 1, dims, 3);
  Term addr = uop_const(DT_INT32, 0);
  Term idx = uop_index_e(buf, addr);
  // INDEX_E + LOAD on an int buffer yields an int; cast is a no-op
  // identity that uop_cast collapses, so return the LOAD directly.
  return uop_load(idx);
}

// sym1: ADD(x, CONST(0.0)) -> x  (and commute)
static int test_sym1_add_zero(void) {
  thvm_init();
  TEST_BEGIN("sym1 ADD(x, 0) -> x");
  Term x = mk_opaque_f32();
  Term zero = uop_const(DT_FP32, f32_to_bits_test(0.0f));
  // Construct via uop_binary which already folds at constructor; but
  // make sure: the OUTPUT of sym must equal x.
  Term lhs = uop_binary(UOP_ADD, x, zero);
  Term rhs = uop_binary(UOP_ADD, zero, x);
  Term lhs_sym = uop_symbolic_rewrite(lhs);
  Term rhs_sym = uop_symbolic_rewrite(rhs);
  CHECK_EQ(lhs_sym, x);
  CHECK_EQ(rhs_sym, x);
  thvm_free();
  TEST_REPORT();
}

// sym2: MUL(x, CONST(1.0)) -> x
static int test_sym2_mul_one(void) {
  thvm_init();
  TEST_BEGIN("sym2 MUL(x, 1) -> x");
  Term x = mk_opaque_f32();
  Term one = uop_const(DT_FP32, f32_to_bits_test(1.0f));
  Term r = uop_symbolic_rewrite(uop_binary(UOP_MUL, x, one));
  Term r2 = uop_symbolic_rewrite(uop_binary(UOP_MUL, one, x));
  CHECK_EQ(r, x);
  CHECK_EQ(r2, x);
  thvm_free();
  TEST_REPORT();
}

// sym3: MUL(x, CONST(0.0)) -> CONST(0.0).  SCALAR-LANE assumption: the
// devectorizer has already split shape; per-lane scalar MUL by 0 is
// safely 0 (the original tensor-level MUL kept x's shape and broadcast
// 0, but post-devectorize lanes are shape-less).
static int test_sym3_mul_zero(void) {
  thvm_init();
  TEST_BEGIN("sym3 MUL(x, 0) -> 0");
  Term x = mk_opaque_f32();
  Term zero = uop_const(DT_FP32, f32_to_bits_test(0.0f));
  Term r = uop_symbolic_rewrite(uop_binary(UOP_MUL, x, zero));
  Term r2 = uop_symbolic_rewrite(uop_binary(UOP_MUL, zero, x));
  CHECK(is_const_zero_f32(r));
  CHECK(is_const_zero_f32(r2));
  thvm_free();
  TEST_REPORT();
}

// sym4: ADD(CONST(a), CONST(b)) -> CONST(a+b)
static int test_sym4_const_add(void) {
  thvm_init();
  TEST_BEGIN("sym4 ADD(CONST(a), CONST(b)) -> CONST(a+b)");
  Term a = uop_const(DT_FP32, f32_to_bits_test(2.0f));
  Term b = uop_const(DT_FP32, f32_to_bits_test(3.0f));
  Term r = uop_symbolic_rewrite(uop_binary(UOP_ADD, a, b));
  CHECK(is_const_with_bits(r, f32_to_bits_test(5.0f)));
  thvm_free();
  TEST_REPORT();
}

// sym5: MUL(CONST(a), CONST(b)) -> CONST(a*b)
static int test_sym5_const_mul(void) {
  thvm_init();
  TEST_BEGIN("sym5 MUL(CONST(a), CONST(b)) -> CONST(a*b)");
  Term a = uop_const(DT_FP32, f32_to_bits_test(2.5f));
  Term b = uop_const(DT_FP32, f32_to_bits_test(4.0f));
  Term r = uop_symbolic_rewrite(uop_binary(UOP_MUL, a, b));
  CHECK(is_const_with_bits(r, f32_to_bits_test(10.0f)));
  thvm_free();
  TEST_REPORT();
}

// sym6: NEG(NEG(x)) -> x
static int test_sym6_neg_neg(void) {
  thvm_init();
  TEST_BEGIN("sym6 NEG(NEG(x)) -> x");
  Term x = mk_opaque_f32();
  Term r = uop_symbolic_rewrite(uop_unary(UOP_NEG, uop_unary(UOP_NEG, x)));
  CHECK_EQ(r, x);
  thvm_free();
  TEST_REPORT();
}

// sym7: RECIP(RECIP(x)) -> x
static int test_sym7_recip_recip(void) {
  thvm_init();
  TEST_BEGIN("sym7 RECIP(RECIP(x)) -> x");
  Term x = mk_opaque_f32();
  Term r = uop_symbolic_rewrite(uop_unary(UOP_RECIP, uop_unary(UOP_RECIP, x)));
  CHECK_EQ(r, x);
  thvm_free();
  TEST_REPORT();
}

// sym8: Integer IADD(x, 0) -> x
static int test_sym8_iadd_zero(void) {
  thvm_init();
  TEST_BEGIN("sym8 IADD(x, 0) -> x");
  Term x = mk_opaque_i32();
  Term zero = uop_const(DT_INT32, 0);
  Term r = uop_symbolic_rewrite(uop_int_binary(UOP_IADD, x, zero));
  Term r2 = uop_symbolic_rewrite(uop_int_binary(UOP_IADD, zero, x));
  CHECK_EQ(r, x);
  CHECK_EQ(r2, x);
  thvm_free();
  TEST_REPORT();
}

// sym9: Integer IMUL(x, 1) -> x
static int test_sym9_imul_one(void) {
  thvm_init();
  TEST_BEGIN("sym9 IMUL(x, 1) -> x");
  Term x = mk_opaque_i32();
  Term one = uop_const(DT_INT32, 1);
  Term r = uop_symbolic_rewrite(uop_int_binary(UOP_IMUL, x, one));
  Term r2 = uop_symbolic_rewrite(uop_int_binary(UOP_IMUL, one, x));
  CHECK_EQ(r, x);
  CHECK_EQ(r2, x);
  thvm_free();
  TEST_REPORT();
}

// sym10: Integer IMUL(x, 0) -> CONST(0)
static int test_sym10_imul_zero(void) {
  thvm_init();
  TEST_BEGIN("sym10 IMUL(x, 0) -> 0");
  Term x = mk_opaque_i32();
  Term zero = uop_const(DT_INT32, 0);
  Term r = uop_symbolic_rewrite(uop_int_binary(UOP_IMUL, x, zero));
  Term r2 = uop_symbolic_rewrite(uop_int_binary(UOP_IMUL, zero, x));
  CHECK(is_const_with_bits(r, 0));
  CHECK(is_const_with_bits(r2, 0));
  thvm_free();
  TEST_REPORT();
}

// sym11: Nested integer constant fold IADD(IMUL(2,3), IADD(4,5)) -> CONST(15)
static int test_sym11_nested_int_fold(void) {
  thvm_init();
  TEST_BEGIN("sym11 nested int constant fold -> single CONST");
  Term c2 = uop_const(DT_INT32, 2);
  Term c3 = uop_const(DT_INT32, 3);
  Term c4 = uop_const(DT_INT32, 4);
  Term c5 = uop_const(DT_INT32, 5);
  Term m  = uop_int_binary(UOP_IMUL, c2, c3);       // 6
  Term a  = uop_int_binary(UOP_IADD, c4, c5);       // 9
  Term r  = uop_symbolic_rewrite(uop_int_binary(UOP_IADD, m, a));
  CHECK(is_const_with_bits(r, 15));
  thvm_free();
  TEST_REPORT();
}

// sym12: GEP(STACK(s0, s1, s2), (i,)) -> s_i
static int test_sym12_gep_stack(void) {
  thvm_init();
  TEST_BEGIN("sym12 GEP(STACK(...), (i,)) -> s_i");
  Term s0 = mk_opaque_f32();
  Term s1 = mk_opaque_f32_b();
  Term zero = uop_const(DT_FP32, f32_to_bits_test(0.0f));
  Term s2 = uop_binary(UOP_ADD, s0, s1);
  Term srcs[3] = { s0, s1, s2 };
  Term st = uop_stack(3, srcs);
  u32 idx0 = 0, idx1 = 1, idx2 = 2;
  Term g0 = uop_gep(st, 1, &idx0);
  Term g1 = uop_gep(st, 1, &idx1);
  Term g2 = uop_gep(st, 1, &idx2);
  CHECK_EQ(uop_symbolic_rewrite(g0), s0);
  CHECK_EQ(uop_symbolic_rewrite(g1), s1);
  CHECK_EQ(uop_symbolic_rewrite(g2), s2);
  (void)zero;
  thvm_free();
  TEST_REPORT();
}

// sym13: Idempotent.  Run sym, then again; second call returns the
// same Term (no further rewrite).
static int test_sym13_idempotent(void) {
  thvm_init();
  TEST_BEGIN("sym13 sym(sym(x)) == sym(x)");
  Term x = mk_opaque_f32();
  Term zero = uop_const(DT_FP32, f32_to_bits_test(0.0f));
  Term one = uop_const(DT_FP32, f32_to_bits_test(1.0f));
  // ADD(MUL(x, 1), 0) -- two-level simplification.
  Term inner = uop_binary(UOP_MUL, x, one);
  Term outer = uop_binary(UOP_ADD, inner, zero);
  Term r1 = uop_symbolic_rewrite(outer);
  Term r2 = uop_symbolic_rewrite(r1);
  CHECK_EQ(r1, x);
  CHECK_EQ(r2, r1);
  thvm_free();
  TEST_REPORT();
}

// sym14: Walk under STACK to fold each lane.
static int test_sym14_recursive_under_stack(void) {
  thvm_init();
  TEST_BEGIN("sym14 recursive: STACK(MUL(x_i, 0)) -> STACK(0, 0, ...)");
  Term zero = uop_const(DT_FP32, f32_to_bits_test(0.0f));
  Term lanes[4];
  for (u32 i = 0; i < 4; i++) {
    Term xi = mk_opaque_f32();  // shared (hash-cons) but binary distinct
    lanes[i] = uop_binary(UOP_MUL, xi, zero);
  }
  Term st = uop_stack(4, lanes);
  Term r = uop_symbolic_rewrite(st);
  // Every lane in r must be CONST(0).
  CHECK(term_tag(r) == TAG_UOP && term_ext(r) == UOP_STACK);
  CHECK_EQ(uop_stack_n(r), 4);
  for (u32 i = 0; i < 4; i++) {
    CHECK(is_const_zero_f32(uop_stack_src(r, i)));
  }
  thvm_free();
  TEST_REPORT();
}

// sym15: No-op preserves identity (when nothing to fold).
static int test_sym15_no_op(void) {
  thvm_init();
  TEST_BEGIN("sym15 no-op preserves the input Term");
  Term x = mk_opaque_f32();
  Term y = mk_opaque_f32_b();
  Term t = uop_binary(UOP_ADD, x, y);
  Term r = uop_symbolic_rewrite(t);
  CHECK_EQ(r, t);
  thvm_free();
  TEST_REPORT();
}

int main(void) {
  int rc = 0;
  rc |= test_sym1_add_zero();
  rc |= test_sym2_mul_one();
  rc |= test_sym3_mul_zero();
  rc |= test_sym4_const_add();
  rc |= test_sym5_const_mul();
  rc |= test_sym6_neg_neg();
  rc |= test_sym7_recip_recip();
  rc |= test_sym8_iadd_zero();
  rc |= test_sym9_imul_one();
  rc |= test_sym10_imul_zero();
  rc |= test_sym11_nested_int_fold();
  rc |= test_sym12_gep_stack();
  rc |= test_sym13_idempotent();
  rc |= test_sym14_recursive_under_stack();
  rc |= test_sym15_no_op();
  return rc;
}
