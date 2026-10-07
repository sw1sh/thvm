// Reduce over a broadcast EXPAND to {S,S}.  ramp{S}=[0..S-1] reshaped to
// {1,S} or {S,1} and expanded to {S,S}, then summed (or maxed) over both
// axes.  The broadcast axis indexes its source at a constant 0, so its
// RANGE leaves the reduce body; tinygrad drops such an "unparented" range
// from the REDUCE and multiplies a SUM by its extent
// (codegen/simplify.py:81-91 reduce_unparented).  Runtime S=4: the sums are
// 4 * (0+1+2+3) = 24 and the max is 3.  The static-extent cases check the
// same rule with a literal extent.
#include "../src/thvm.c"
#include "test.h"

static u32 ramp_tid(u32 hi, u32 s) {
  Shape rs = {0};
  rs.ndim = 1;
  rs.dims[0] = hi;
  u32 t = tensor_alloc(&CPU_BACKEND, rs, DT_FP32);
  float r[64];
  for (u32 i = 0; i < hi; i++) {
    r[i] = (float)i;
  }
  CPU_BACKEND.buf_write(TENS[t].buf_id, r, hi * 4);
  if (s != 0) {
    TENS[t].view.shape.dims[0] = kvar_pack_extent(s);
  }
  return t;
}

static float reduce_all(u32 kind, Term t, u32 s, u32 bound) {
  u32 ax[2] = {0, 1};
  Term red = uop_reduce_multi(kind, 2, ax, t);
  if (s != 0) {
    kvar_set_runtime(s, bound);
  }
  Term r = term_resolve(thvm_realize(red));
  float o = -2;
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, &o, 4);
  return o;
}

// ramp{n} (n = the kvar `s`'s packed extent, or a literal n when s == 0)
// reshaped to `inner ? {n,1} : {1,n}` and expanded to {n,n}.
static Term broadcast_square(u32 hi, u32 s, u32 n, int inner) {
  u32 P = s != 0 ? kvar_pack_extent(s) : n;
  u32 rs[2] = { inner ? P : 1u, inner ? 1u : P };
  u32 ss[2] = { P, P };
  Term ramp = term_new(0, TAG_TEN, DT_FP32, ramp_tid(hi, s));
  return uop_expand(uop_reshape(ramp, 2, rs), 2, ss);
}

static void check_close(float got, float want) {
  if (!(got > want - 0.1f && got < want + 0.1f)) {
    fprintf(stderr, "  %s: got %g want %g\n", test_name, got, want);
  }
  CHECK(got > want - 0.1f && got < want + 0.1f);
}

int main(void) {
  thvm_init();
  u32 hi = 16;

  TEST_BEGIN("sym-expand/sum-broadcast-outer");
  {
    u32 s = kvar_alloc("a", 1, hi);
    check_close(reduce_all(REDUCE_SUM, broadcast_square(hi, s, 0, 0), s, 4), 24.0f);
  }
  TEST_BEGIN("sym-expand/sum-broadcast-inner");
  {
    u32 s = kvar_alloc("b", 1, hi);
    check_close(reduce_all(REDUCE_SUM, broadcast_square(hi, s, 0, 1), s, 4), 24.0f);
  }
  TEST_BEGIN("sym-expand/max-broadcast-inner");
  {
    u32 s = kvar_alloc("c", 1, hi);
    check_close(reduce_all(REDUCE_MAX, broadcast_square(hi, s, 0, 1), s, 4), 3.0f);
  }
  TEST_BEGIN("static-expand/sum-broadcast-inner");
  check_close(reduce_all(REDUCE_SUM, broadcast_square(4, 0, 4, 1), 0, 0), 24.0f);
  TEST_BEGIN("static-expand/sum-broadcast-outer");
  check_close(reduce_all(REDUCE_SUM, broadcast_square(4, 0, 4, 0), 0, 0), 24.0f);

  thvm_free();
  TEST_REPORT();
}
