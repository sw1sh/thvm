// uop_dag_store_full_bytes: the bytes a STORE writes when its address visits
// every element of its BUFFER exactly once, else 0.  The JIT replay skips the
// zero fill of a recycled output only on a non-zero answer, so a store that
// leaves any element unwritten must give 0.
#include "../src/thvm.c"
#include "test.h"

static Term cint(u32 v) { return uop_const(DT_INT32, v); }
static Term mul(Term a, u32 c) { return uop_int_binary(UOP_IMUL, a, cint(c)); }
static Term add(Term a, Term b) { return uop_int_binary(UOP_IADD, a, b); }

int main(void) {
  thvm_init();
  Term value = uop_const(DT_FP32, 0x3F800000u);

  u32 d48[2] = {4, 8};
  Term buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, d48);
  Term row  = uop_range(0, KAX_LOOP, 4);
  Term col  = uop_range(1, KAX_LOOP, 8);

  TEST_BEGIN("store-full/row-major-address-covers");
  CHECK_EQ(uop_dag_store_full_bytes(uop_store(buf, add(mul(row, 8), col), value)), 4 * 8 * 4);

  TEST_BEGIN("store-full/operand-order-does-not-matter");
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf, add(col, uop_int_binary(UOP_IMUL, cint(8), row)), value)),
           4 * 8 * 4);

  TEST_BEGIN("store-full/two-byte-elements");
  Term buf16 = uop_buffer(UOP_SCOPE_GLOBAL, DT_BF16, 2, d48);
  CHECK_EQ(uop_dag_store_full_bytes(uop_store(buf16, add(mul(row, 8), col), value)), 4 * 8 * 2);

  // An axis split by a kernel opt: 8 columns as 2 outer by 4 inner, the inner
  // range under an UPCAST annotation.
  TEST_BEGIN("store-full/split-axis-under-an-opt-covers");
  Term outer = uop_range(2, KAX_GLOBAL, 2);
  Term inner = uop_opt(uop_range(3, KAX_UPCAST, 4), UOP_OPT_UPCAST, 4);
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf, add(mul(row, 8), add(mul(outer, 4), inner)), value)),
           4 * 8 * 4);

  TEST_BEGIN("store-full/nested-products-cover");
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf, add(mul(add(mul(row, 2), outer), 4), uop_range(3, KAX_LOOP, 4)), value)),
           4 * 8 * 4);

  // A size-1 axis reads 0 whatever its stride.
  TEST_BEGIN("store-full/unit-axis-is-ignored");
  u32 d18[2] = {1, 8};
  Term buf18 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, d18);
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf18, add(mul(uop_range(4, KAX_LOOP, 1), 8), col), value)),
           8 * 4);

  TEST_BEGIN("store-full/window-with-an-offset-does-not");
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf, add(add(mul(row, 8), uop_range(5, KAX_LOOP, 4)), cint(4)), value)),
           0);

  TEST_BEGIN("store-full/fewer-rows-than-the-buffer-does-not");
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf, add(mul(uop_range(6, KAX_LOOP, 2), 8), col), value)),
           0);

  TEST_BEGIN("store-full/a-skipping-stride-does-not");
  u32 d16[1] = {16};
  Term buf1 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, d16);
  CHECK_EQ(uop_dag_store_full_bytes(uop_store(buf1, mul(uop_range(7, KAX_LOOP, 8), 2), value)), 0);

  TEST_BEGIN("store-full/overlapping-strides-do-not");
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf, add(mul(row, 4), col), value)),
           0);

  TEST_BEGIN("store-full/division-in-the-address-is-not-proven");
  CHECK_EQ(uop_dag_store_full_bytes(
               uop_store(buf1, uop_int_binary(UOP_IDIV, uop_range(8, KAX_LOOP, 32), cint(2)), value)),
           0);

  TEST_BEGIN("store-full/not-a-store");
  CHECK_EQ(uop_dag_store_full_bytes(value), 0);
  CHECK_EQ(uop_dag_store_full_bytes(0), 0);

  thvm_free();
  TEST_REPORT();
}
