// test_simplify_ranges.c - uop_simplify_ranges, tinygrad's pm_simplify_ranges
// (codegen/simplify.py:21-38) on a kernel's output ranges: adjacent ranges
// that every address reads row-major merge into one; a transposed or
// broadcast read, which would need an IDIV or IMOD, keeps them apart.

#include "../src/thvm.c"
#include "test.h"

static Term kc(u32 v) { return uop_const(DT_INT32, v); }

static Term affine2(Term a, u32 sa, Term b) {
  return uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, a, kc(sa)), b);
}

static Term buf(u32 inst, u32 numel) {
  u32 dims[1] = { numel };
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, inst);
}

static u32 loop_count(Term root) {
  Term loops[MAX_AXES];
  return simplify_ranges_loops(root, loops, MAX_AXES);
}

static void test_row_major_copy_merges(void) {
  Term a0   = uop_range(0, KAX_LOOP, 256);
  Term a1   = uop_range(1, KAX_LOOP, 16);
  Term addr = affine2(a0, 16, a1);
  Term root = uop_store(buf(0, 4096), addr, uop_index_e(buf(1, 4096), addr));
  CHECK_EQ(loop_count(root), 2);
  Term out = uop_simplify_ranges(root);
  CHECK_EQ(loop_count(out), 1);
  CHECK(heap_read(term_val(out) + 1) == uop_range(0, KAX_LOOP, 4096));
}

static void test_transposed_read_keeps_ranges(void) {
  Term a0   = uop_range(0, KAX_LOOP, 256);
  Term a1   = uop_range(1, KAX_LOOP, 16);
  Term root = uop_store(buf(0, 4096), affine2(a0, 16, a1),
                        uop_index_e(buf(1, 4096), affine2(a1, 256, a0)));
  CHECK(uop_simplify_ranges(root) == root);
}

static void test_broadcast_read_keeps_ranges(void) {
  // out[a0, a1, a2] = in[a0, a2]: merging (a1, a2) leaves in's a2 = R % 8,
  // merging (a0, a1) leaves in's a0 = R / 8.
  Term a0   = uop_range(0, KAX_LOOP, 4);
  Term a1   = uop_range(1, KAX_LOOP, 8);
  Term a2   = uop_range(2, KAX_LOOP, 8);
  Term out  = uop_int_binary(UOP_IADD, affine2(a0, 64, uop_int_binary(UOP_IMUL, a1, kc(8))), a2);
  Term root = uop_store(buf(0, 256), out, uop_index_e(buf(1, 32), affine2(a0, 8, a2)));
  CHECK_EQ(loop_count(uop_simplify_ranges(root)), 3);
}

static void test_reduce_output_merges(void) {
  // out[a0, a1] = sum_k in[a0, a1, k]: the output ranges merge, the reduce
  // range stays.
  Term a0   = uop_range(0, KAX_LOOP,   256);
  Term a1   = uop_range(1, KAX_LOOP,   16);
  Term k    = uop_range(2, KAX_REDUCE, 32);
  Term row  = affine2(a0, 16, a1);
  Term red  = uop_reduce(REDUCE_SUM, 2, uop_index_e(buf(1, 4096 * 32), affine2(row, 32, k)));
  Term root = uop_store(buf(0, 4096), row, red);
  Term out  = uop_simplify_ranges(root);
  CHECK_EQ(loop_count(out), 1);
  CHECK(heap_read(term_val(out) + 1) == uop_range(0, KAX_LOOP, 4096));
  Term v = heap_read(term_val(out) + 2);
  CHECK(term_ext(v) == UOP_REDUCE && uop_reduce_axis(v, 0) == 2);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("row_major_copy_merges");       test_row_major_copy_merges();
  TEST_BEGIN("transposed_read_keeps_ranges"); test_transposed_read_keeps_ranges();
  TEST_BEGIN("broadcast_read_keeps_ranges");  test_broadcast_read_keeps_ranges();
  TEST_BEGIN("reduce_output_merges");        test_reduce_output_merges();
  TEST_REPORT();
}
