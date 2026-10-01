// test_expand_shared_view.c -- a broadcast EXPAND shared by consumers that
// read it through different views.
//
// `x + b` broadcasts b (D) to (N, D) through EXPAND(RESHAPE(b)), and that
// one hash-consed EXPAND serves every add of b.  When the adds reach the
// output through different views of b's own axis (the two halves of a cat
// along dim 1, two shrink offsets), the consumers disagree on that axis and
// each must index b with its own range.  A shared EXPAND is spliced into
// each consumer by rebinding the RANGEs of its shared ranges
// (ru_build_axis_subst), so a compound shared range (a PAD's valid, a
// SHRINK offset) carried one consumer's index into the other consumer's
// kernel.  On b's broadcast axis the consumers may disagree freely (the
// source reads row 0 there), and that EXPAND stays a fused view.
//
//   1. (x + b).pad(cols 0..4 of 7) * 2 + (y + b).pad(cols 3..7 of 7): the
//      dim-1 cat of two adds of one bias;
//   2. the same through shrink offsets: (x + b)[:, 1:3] + (y + b)[:, 2:4];
//   3. a computed broadcast source, b * 3, shared the same way;
//   4. the shared EXPAND's range on b's axis is a RANGE every consumer
//      rebinds, and a divergence on the broadcast axis only (a dim-0 cat)
//      still runs as one kernel.
// Values compare byte for byte with a host reference.  Run the binary as is
// (C JIT) and with THVM_CPU_JIT_WARMUP=1000000000 (UOp walker).
#include "../src/thvm.c"
#include "test.h"

#define N 4
#define D 4

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

static Term fconst(u32 ndim, u32 const *dims, f32 v) {
  u32 bits;
  memcpy(&bits, &v, sizeof bits);
  u32 ones[MAX_DIM] = { 1, 1, 1, 1, 1, 1, 1, 1 };
  return uop_expand(uop_reshape(uop_const(DT_FP32, bits), ndim, ones), ndim, dims);
}

static Term bias_view(Term b) {
  u32 r[2] = { 1, D };
  u32 e[2] = { N, D };
  return uop_expand(uop_reshape(b, 2, r), 2, e);
}

static Term pad2(Term t, u32 r0, u32 r1, u32 c0, u32 c1) {
  u32 be[4] = { r0, r1, c0, c1 };
  return uop_pad(t, 2, be);
}

static Term shrink2(Term t, u32 r0, u32 r1, u32 c0, u32 c1) {
  u32 be[4] = { r0, r1, c0, c1 };
  return uop_shrink(t, 2, be);
}

static void realize_check(Term root, f32 const *want, u32 n, char const *what) {
  Term done = term_resolve(thvm_realize(root));
  CHECK_EQ(term_tag(done), TAG_TEN);
  if (term_tag(done) != TAG_TEN) {
    return;
  }
  f32 got[64] = {0};
  CPU_BACKEND.buf_read(TENS[term_val(done)].buf_id, got, (u64)n * sizeof(f32));
  u32 shown = 0;
  for (u32 i = 0; i < n && shown < 6; i++) {
    if (memcmp(&got[i], &want[i], sizeof(f32)) != 0) {
      fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", what, i, got[i], want[i]);
      shown++;
    }
  }
  CHECK(memcmp(got, want, (size_t)n * sizeof(f32)) == 0);
}

static f32 xs[N * D], ys[N * D], bs[D];

// Fresh leaves per case: a realize may rewrite the nodes it consumed.
static void leaves(Term *x, Term *y, Term *b) {
  u32 nd[2] = { N, D };
  u32 bd[1] = { D };
  *x = ten(mk(2, nd, xs));
  *y = ten(mk(2, nd, ys));
  *b = ten(mk(1, bd, bs));
}

int main(void) {
  thvm_init();

  for (u32 i = 0; i < N * D; i++) {
    xs[i] = 0.25f * (f32)i - 1.5f;
    ys[i] = 2.0f - 0.375f * (f32)i;
  }
  for (u32 j = 0; j < D; j++) {
    bs[j] = 10.0f * (f32)(j + 1);
  }
  u32 bd[1] = { D };
  Term x, y, b;

  TEST_BEGIN("expand-shared-view/dim1-cat-of-two-bias-adds");
  {
    leaves(&x, &y, &b);
    Term bx = uop_binary(UOP_ADD, x, bias_view(b));
    Term by = uop_binary(UOP_ADD, y, bias_view(b));
    u32 od[2] = { N, 7 };
    Term root = uop_binary(UOP_ADD,
                           uop_binary(UOP_MUL, pad2(bx, 0, 0, 0, 3), fconst(2, od, 2.0f)),
                           pad2(by, 0, 0, 3, 0));
    f32 want[N * 7];
    for (u32 r = 0; r < N; r++) {
      for (u32 c = 0; c < 7; c++) {
        f32 lhs = (c < D) ? (xs[r * D + c] + bs[c]) * 2.0f : 0.0f;
        f32 rhs = (c >= 3) ? ys[r * D + c - 3] + bs[c - 3] : 0.0f;
        want[r * 7 + c] = lhs + rhs;
      }
    }
    realize_check(root, want, N * 7, "dim1 cat");
  }

  TEST_BEGIN("expand-shared-view/two-shrink-offsets");
  {
    leaves(&x, &y, &b);
    Term bx = uop_binary(UOP_ADD, x, bias_view(b));
    Term by = uop_binary(UOP_ADD, y, bias_view(b));
    Term root = uop_binary(UOP_ADD, shrink2(bx, 0, N, 1, 3), shrink2(by, 0, N, 2, 4));
    f32 want[N * 2];
    for (u32 r = 0; r < N; r++) {
      for (u32 c = 0; c < 2; c++) {
        want[r * 2 + c] = (xs[r * D + c + 1] + bs[c + 1]) + (ys[r * D + c + 2] + bs[c + 2]);
      }
    }
    realize_check(root, want, N * 2, "shrink offsets");
  }

  TEST_BEGIN("expand-shared-view/computed-broadcast-source");
  {
    leaves(&x, &y, &b);
    Term b3 = uop_binary(UOP_MUL, b, fconst(1, bd, 3.0f));
    Term bx = uop_binary(UOP_ADD, x, bias_view(b3));
    Term by = uop_binary(UOP_ADD, y, bias_view(b3));
    Term root = uop_binary(UOP_ADD, pad2(bx, 0, 0, 0, 3), pad2(by, 0, 0, 3, 0));
    f32 want[N * 7];
    for (u32 r = 0; r < N; r++) {
      for (u32 c = 0; c < 7; c++) {
        f32 lhs = (c < D) ? xs[r * D + c] + bs[c] * 3.0f : 0.0f;
        f32 rhs = (c >= 3) ? ys[r * D + c - 3] + bs[c - 3] * 3.0f : 0.0f;
        want[r * 7 + c] = lhs + rhs;
      }
    }
    realize_check(root, want, N * 7, "computed source");
  }

  TEST_BEGIN("expand-shared-view/shared-range-is-rebindable");
  {
    leaves(&x, &y, &b);
    Term view = bias_view(b);
    Term root = uop_binary(UOP_ADD,
                           pad2(uop_binary(UOP_ADD, x, view), 0, 0, 0, 3),
                           pad2(uop_binary(UOP_ADD, y, view), 0, 0, 3, 0));
    bufferize_classify(root);
    run_rangeify_unified(root);
    u32 vi = bufferize_info_find(term_val(view));
    CHECK(vi != 0xFFFFFFFFu);
    if (vi != 0xFFFFFFFFu) {
      CHECK_EQ(rangeify_unified_has_ranges_at(vi), 1);
      CHECK_EQ(rangeify_unified_is_realized(vi), 0);
      Term r1 = rangeify_unified_out_rng_at(vi, 1);
      CHECK(term_tag(r1) == TAG_UOP && term_ext(r1) == UOP_RANGE);
    }
  }

  TEST_BEGIN("expand-shared-view/broadcast-axis-divergence-stays-fused");
  {
    leaves(&x, &y, &b);
    Term bx = uop_binary(UOP_ADD, x, bias_view(b));
    Term by = uop_binary(UOP_ADD, y, bias_view(b));
    Term root = uop_binary(UOP_ADD, pad2(bx, 0, N, 0, 0), pad2(by, N, 0, 0, 0));
    f32 want[2 * N * D];
    for (u32 r = 0; r < 2 * N; r++) {
      for (u32 c = 0; c < D; c++) {
        want[r * D + c] = (r < N) ? xs[r * D + c] + bs[c] : ys[(r - N) * D + c] + bs[c];
      }
    }
    u32 k0 = KERNELS_NEXT;
    realize_check(root, want, 2 * N * D, "dim0 cat");
    CHECK_EQ(KERNELS_NEXT - k0, 1);
  }

  TEST_REPORT();
}
