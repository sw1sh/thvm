// test_view_pad.c - documents the design choice for sub-item
// f3f of the kernel-fusion arc: PAD does NOT take a view-only
// fast path.
//
// Rationale: PAD must materialize zeros in the padded region.
// A view-only alias with negative offset would have to read
// out-of-bounds memory before the allocated buffer; even though
// cpu_buf_alloc uses calloc, the memory BEFORE the buffer is
// not ours to read.  So PAD falls through to cpu_op_pad
// (memcpy + zero-fill), which is correct + safe.
//
// This test verifies:
//   - PAD continues to allocate a fresh kernel-output buffer
//     (not a view alias).
//   - The zeros in the padded region are correct.
//   - The original values land at the right offsets.
//
// If a future commit adds a view-only PAD path that re-uses
// the source buffer, it should ALSO replace this test with a
// view-aware variant.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // 2x2 source: {{1,2},{3,4}} -> PAD with widths
  // [{1,1},{1,1}] -> 4x4 with zero border:
  //   {{0,0,0,0}, {0,1,2,0}, {0,3,4,0}, {0,0,0,0}}.
  Shape s = {0}; s.ndim = 2; s.dims[0] = 2; s.dims[1] = 2;
  u32 src_tid = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
  f32 src_buf[4] = {1, 2, 3, 4};
  CURRENT_BACKEND->buf_write(TENS[src_tid].buf_id, src_buf, sizeof(src_buf));
  Term src = term_new(0, TAG_TEN, DT_F32, src_tid);

  u32 widths[4] = {1, 1, 1, 1};   // [b0, e0, b1, e1]
  u32 kernels_before = KERNELS_NEXT;
  u32 tens_before    = TENS_NEXT;

  Term padded = uop_pad(src, 2, widths);
  Term out = materialize_uop_in_env(padded, /*env_id=*/0);

  TEST_BEGIN("view-pad/falls-through-to-kernel-path");
  // Unlike SHRINK / PERMUTE / EXPAND / RESHAPE, PAD's
  // materialize_uop_in_env entry must NOT return a TAG_TEN
  // alias -- it returns a UOP_KERNEL term and allocates a
  // fresh KernelEntry + output TenDesc.
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_KERNEL);
  CHECK_EQ(KERNELS_NEXT, kernels_before + 1);
  CHECK_EQ(TENS_NEXT,    tens_before + 1);

  TEST_BEGIN("view-pad/output-tid-has-fresh-buf");
  // The kernel's output TenDesc is brand new (not aliased).
  u32 kid = (u32)term_val(heap_read(term_val(out) + 1));
  KernelEntry *ke = &KERNELS[kid];
  u32 out_tid = ke->output_tid;
  CHECK(out_tid != src_tid);
  CHECK(TENS[out_tid].buf_id != TENS[src_tid].buf_id);

  TEST_BEGIN("view-pad/fired-kernel-zero-fills-and-copies");
  Term done = wnf(out);
  CHECK_EQ(term_tag(done), TAG_TEN);
  u32 done_tid = (u32)term_val(done);
  f32 padded_buf[16] = {0};
  CURRENT_BACKEND->buf_read(TENS[done_tid].buf_id, padded_buf,
                            sizeof(padded_buf));
  // Border zeros.
  CHECK(padded_buf[0]  == 0.0f);
  CHECK(padded_buf[3]  == 0.0f);
  CHECK(padded_buf[12] == 0.0f);
  CHECK(padded_buf[15] == 0.0f);
  // Interior values at offsets (1,1)=5, (1,2)=6, (2,1)=9, (2,2)=10.
  CHECK(padded_buf[5]  == 1.0f);
  CHECK(padded_buf[6]  == 2.0f);
  CHECK(padded_buf[9]  == 3.0f);
  CHECK(padded_buf[10] == 4.0f);

  thvm_free();
  TEST_REPORT();
}
