// test_uop_mov_cache_growth.c -- the movement-op hash-cons cache keeps
// hash-consing past its initial 2^18 slots.
//
// uop_buffer_inst, the movement ops and REDUCE hash-cons through
// uop_mov_cache.  A fixed table that filled up dropped every later insert,
// so two equal UOP_BUFFERs built after that were distinct Terms; the CPU
// renderer names a kernel's input slots by BUFFER identity, and a second
// copy of one input pushed the others to higher slots with the wrong C type
// (a long py test run then read float gradients as int bits).  Here 300000
// distinct BUFFERs fill the initial table past capacity; equal requests
// made afterwards, and the first one made, must still return one Term, and
// a realize still computes.
#include "../src/thvm.c"
#include "test.h"

#define N_FILL 300000u

int main(void) {
  thvm_init();

  TEST_BEGIN("uop-mov-cache/hash-cons-past-initial-capacity");
  {
    u32  d0    = 1;
    Term first = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, &d0, 1);
    for (u32 i = 0; i < N_FILL; i++) {
      u32 d = i + 2;
      uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, &d, 1);
    }
    u32  d = 7;
    Term a = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_INT32, 1, &d, 4);
    Term b = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_INT32, 1, &d, 4);
    CHECK_EQ(a, b);
    CHECK_EQ(uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, &d0, 1), first);
    u32 last = N_FILL + 1;
    CHECK_EQ(uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, &last, 1),
             uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, &last, 1));
  }

  TEST_BEGIN("uop-mov-cache/realize-after-growth");
  {
    f32   xs[4] = { 1.0f, -2.0f, 3.5f, 0.25f };
    Shape s     = {0};
    s.ndim    = 2;
    s.dims[0] = 2;
    s.dims[1] = 2;
    u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
    CPU_BACKEND.buf_write(TENS[tid].buf_id, xs, sizeof xs);
    Term x  = term_new(0, TAG_TEN, DT_FP32, tid);
    u32  pm[2] = { 1, 0 };
    Term y  = uop_binary(UOP_ADD, x, uop_permute(x, 2, pm));
    Term r  = term_resolve(thvm_realize(y));
    CHECK_EQ(term_tag(r), TAG_TEN);
    if (term_tag(r) == TAG_TEN) {
      f32 got[4] = {0};
      CPU_BACKEND.buf_read(TENS[term_val(r)].buf_id, got, sizeof got);
      f32 want[4] = { 2.0f, 1.5f, 1.5f, 0.5f };
      CHECK(memcmp(got, want, sizeof got) == 0);
    }
  }

  TEST_REPORT();
}
