// test_shared_weight_levels.c -- one weight read by a product at two
// levels of a table grown row by row.
//
// The ATP history table (tools/atp_enigma/history_net.py HistoryNet.table)
// appends each level's rows with a dim-0 cat (pad + add) and computes them
// from the earlier rows through the same Linear:
//
//   Z1 = sum_k T0[r, k] W[j, k]            T0: (1, D), W: (D, D)
//   T1 = pad(T0, rows 0..1 of 2) + pad(Z1, rows 1..2 of 2)
//   Z2 = sum_k T1[r, k] W[j, k]
//
// with the products as tinygrad's matmul builds them, x.reshape(r, 1, D) *
// W.reshape(1, D, D) (EXPANDed to r rows) reduced over the last axis, so
// one hash-consed RESHAPE of W feeds both reduces.  Z2's row 1 reads Z1.
// Values compare against a host loop.  Run the binary as is (C JIT) and
// with THVM_CPU_JIT_WARMUP=1000000000 (UOp walker).
#include "../src/thvm.c"
#include "test.h"

#define D 2

static u32 mk(u32 ndim, u32 const *dims, f32 const *data) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data,
                        (u64)TENS[tid].view.numel * sizeof(f32));
  return tid;
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

// x (r, D) times W^T: x.reshape(r, 1, D) * W.reshape(1, D, D), summed over
// the last axis, as py's matmul broadcasts it.
static Term mm(Term x, Term w, u32 r) {
  u32 xr[3] = { r, 1, D };
  u32 wr[3] = { 1, D, D };
  u32 e[3]  = { r, D, D };
  Term xv = uop_expand(uop_reshape(x, 3, xr), 3, e);
  Term wv = uop_reshape(w, 3, wr);
  if (r != 1) {
    wv = uop_expand(wv, 3, e);
  }
  return uop_reduce(REDUCE_SUM, 2, uop_binary(UOP_MUL, xv, wv));
}

static void host_mm(f32 const *x, f32 const *w, u32 r, f32 *out) {
  for (u32 i = 0; i < r; i++) {
    for (u32 j = 0; j < D; j++) {
      f32 acc = 0.0f;
      for (u32 k = 0; k < D; k++) {
        acc += x[i * D + k] * w[j * D + k];
      }
      out[i * D + j] = acc;
    }
  }
}

int main(void) {
  thvm_init();

  f32 ws[D * D] = { 0.5f, -1.25f, 2.0f, 0.75f };
  f32 ts[D]     = { 1.5f, -0.5f };
  u32 wd[2] = { D, D };
  u32 td[2] = { 1, D };

  TEST_BEGIN("shared-weight-levels/two-levels-one-weight");
  {
    Term w  = ten(mk(2, wd, ws));
    Term t0 = ten(mk(2, td, ts));
    Term z1 = mm(t0, w, 1);
    u32 p0[4] = { 0, 1, 0, 0 };
    u32 p1[4] = { 1, 0, 0, 0 };
    Term t1 = uop_binary(UOP_ADD, uop_pad(t0, 2, p0), uop_pad(z1, 2, p1));
    Term z2 = mm(t1, w, 2);
    f32 h1[2 * D], h2[2 * D];
    memcpy(h1, ts, sizeof ts);
    host_mm(ts, ws, 1, h1 + D);
    host_mm(h1, ws, 2, h2);
    Term done = term_resolve(thvm_realize(z2));
    CHECK_EQ(term_tag(done), TAG_TEN);
    if (term_tag(done) == TAG_TEN) {
      f32 got[2 * D] = {0};
      CPU_BACKEND.buf_read(TENS[term_val(done)].buf_id, got, sizeof got);
      for (u32 i = 0; i < 2 * D; i++) {
        if (got[i] != h2[i]) {
          fprintf(stderr, "  out[%u] got=%.9g want=%.9g\n", i, got[i], h2[i]);
        }
      }
      CHECK(memcmp(got, h2, sizeof got) == 0);
    }
  }

  TEST_REPORT();
}
