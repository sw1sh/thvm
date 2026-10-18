// test_hashcons_realize.c -- a construction after a realize builds a fresh
// node.  A realize rewrites nodes in place (materialize leaves an ASSIGN's src
// as the kernel it compiled, wnf stores a reduced child into its parent), and
// that kernel's output dies with the realize, so a later equal construction
// must not get the rewritten node back.  The case is an optimizer's bias
// correction advance, b := b * 0.9, built and realized once per step.
#include "../src/thvm.c"
#include "test.h"

static Term advance(Term b) {
  f32 k = 0.9f;
  u32 bits;
  memcpy(&bits, &k, sizeof bits);
  return uop_binary(UOP_ASSIGN, b, uop_binary(UOP_MUL, b, uop_const(DT_FP32, bits)));
}

int main(void) {
  thvm_init();

  TEST_BEGIN("hashcons/an-equal-assign-after-a-realize-fires-again");
  {
    Shape s   = {0};
    s.ndim    = 1;
    s.dims[0] = 1;
    u32 tid   = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
    f32 one   = 1.0f;
    CPU_BACKEND.buf_write(TENS[tid].buf_id, &one, sizeof one);
    Term b = term_new(0, TAG_TEN, DT_FP32, tid);
    f32  want = 1.0f;
    for (u32 step = 0; step < 3; step++) {
      Term r = thvm_realize(advance(b));
      CHECK(r != 0);
      want *= 0.9f;
      f32 got = 0.0f;
      CPU_BACKEND.buf_read(TENS[tid].buf_id, &got, sizeof got);
      CHECK(got > want - 1e-6f && got < want + 1e-6f);
    }
  }

  thvm_free();
  TEST_REPORT();
}
