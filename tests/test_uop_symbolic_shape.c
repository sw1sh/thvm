// test_uop_symbolic_shape.c - M1 of the symbolic-sequence roadmap
// (docs/plans/decode_roadmap.md): a SYMBOLIC shape dim realized + executed
// at a runtime-bound length.  A {S} tensor (S a kvar in [1,16], buffer
// sized at the upper bound) summed over the symbolic axis yields S -- the
// bound value drives the loop, proving the dormant kvar infra
// (kvar_pack_extent -> rangeify kvar RANGE -> uop_walk kvar_extent_runtime)
// comes to life end to end.
#include "../src/thvm.c"
#include "test.h"

static float sum_symbolic(u32 var_id, u32 bound) {
  Shape sh = {0};
  sh.ndim    = 1;
  sh.dims[0] = kvar_pack_extent(var_id);          // symbolic axis
  u32 tid = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  float ones[16];
  for (int i = 0; i < 16; i++) ones[i] = 1.0f;    // buffer sized at hi=16
  CPU_BACKEND.buf_write(TENS[tid].buf_id, ones, sizeof ones);
  Term sum = uop_reduce(REDUCE_SUM, 0, term_new(0, TAG_TEN, DT_FP32, tid));
  kvar_set_runtime(var_id, bound);
  Term r = term_resolve(thvm_realize(sum));
  float out = -1.0f;
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, &out, sizeof out);
  return out;
}

int main(void) {
  thvm_init();
  int failures = 0;

  // Two distinct symbolic dims bound to different lengths -> sum == length.
  TEST_BEGIN("symbolic-shape/sum-over-kvar-axis");
  {
    u32 s4 = kvar_alloc("s4", 1, 16);
    u32 s8 = kvar_alloc("s8", 1, 16);
    float a = sum_symbolic(s4, 4);
    float b = sum_symbolic(s8, 8);
    printf("  S=4 -> %g (want 4)   S=8 -> %g (want 8)\n", a, b);
    if (!(a > 3.999f && a < 4.001f)) failures++;
    if (!(b > 7.999f && b < 8.001f)) failures++;
  }

  // Rebind the SAME dim across realizes (the generation pattern).  A stale
  // fire-memo would return the first length; a correct re-fire tracks it.
  TEST_BEGIN("symbolic-shape/rebind-same-dim");
  {
    Shape sh = {0}; sh.ndim = 1;
    u32 s = kvar_alloc("s", 1, 16);
    sh.dims[0] = kvar_pack_extent(s);
    u32 tid = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
    float ones[16]; for (int i = 0; i < 16; i++) ones[i] = 1.0f;
    CPU_BACKEND.buf_write(TENS[tid].buf_id, ones, sizeof ones);
    Term sum = uop_reduce(REDUCE_SUM, 0, term_new(0, TAG_TEN, DT_FP32, tid));

    kvar_set_runtime(s, 3);
    Term r3 = term_resolve(thvm_realize(sum));
    float o3 = -1; TENS[(u32)term_val(r3)].backend->buf_read(TENS[(u32)term_val(r3)].buf_id, &o3, sizeof o3);
    kvar_set_runtime(s, 7);
    Term r7 = term_resolve(thvm_realize(sum));
    float o7 = -1; TENS[(u32)term_val(r7)].backend->buf_read(TENS[(u32)term_val(r7)].buf_id, &o7, sizeof o7);
    printf("  rebind S=3 -> %g (want 3)   then S=7 -> %g (want 7)\n", o3, o7);
    if (!(o3 > 2.999f && o3 < 3.001f)) failures++;
    if (!(o7 > 6.999f && o7 < 7.001f)) failures++;   // probes the fire-memo
  }

  printf("  %s  (%d failures)\n", failures == 0 ? "ok" : "FAIL", failures);
  return failures == 0 ? 0 : 1;
}
