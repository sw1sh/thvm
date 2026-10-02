// test_realize_gc_roots.c - what a realize hands back survives it, and a
// heap collection at the realize boundary keeps every Term that outlives
// the realize pointing at its own moved cells.
//
// - A root that another root of the same bundle also reads (Bp's grad
//   feeding W's grad) is held like every root: tinygrad never memory-plans
//   a buffer the call returns (schedule/memory.py:11-12, held_bufs from
//   schedule/__init__.py:303).
// - The collection copies the graph a realize leaves, not the in-place
//   TAG_TEN substitutions the realize made while it ran.
// - TenDesc.grad (a lazy backward graph) moves with the heap.
// - A deferred collection leaves the realize pending and relocates the
//   roots its host passes.
// - A view of a realized kernel output is a leaf, a strided view root
//   waits for its producer, and a stripped kernel is never fired.
#include "../src/thvm.c"
#include "test.h"

#define N 8

static Term tenNN(void) {
  Shape s = {0};
  s.ndim    = 2;
  s.dims[0] = N;
  s.dims[1] = N;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  float data[N * N];
  for (u32 i = 0; i < N * N; i++) {
    data[i] = 0.125f * (float)(i + 1);
  }
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, sizeof data);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static Term tenRC(u32 rows, u32 cols, float scale) {
  Shape s = {0};
  s.ndim    = 2;
  s.dims[0] = rows;
  s.dims[1] = cols;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  float data[rows * cols];
  for (u32 i = 0; i < rows * cols; i++) {
    data[i] = scale * (float)(i + 1);
  }
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, sizeof data);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static int buf_live(Term t) {
  u32 tid = (u32)term_val(term_resolve(t));
  u32 bid = TENS[tid].buf_id;
  return bid != 0 && CPU_BACKEND.buf_refcount(bid) > 0;
}

static void readN(Term t, float out[N]) {
  CPU_BACKEND.buf_read(TENS[(u32)term_val(term_resolve(t))].buf_id, out, N * sizeof(float));
}

// b[i] = sum_j x[i][j]^2 (a reduce boundary); a[j] = sum_i b[j] * x[i][j]
// reads it, broadcast along the rows, through a second reduce.
static void build(Term x, Term *a, Term *b) {
  u32 dnn[2] = {N, N};
  *b = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, x, x));
  *a = uop_reduce(REDUCE_SUM, 0, uop_binary(UOP_MUL, uop_expand(*b, 2, dnn), x));
}

static void expect(float const xs[N * N], float b_out[N], float a_out[N]) {
  for (u32 i = 0; i < N; i++) {
    b_out[i] = 0.0f;
    for (u32 j = 0; j < N; j++) {
      b_out[i] += xs[N * i + j] * xs[N * i + j];
    }
  }
  for (u32 j = 0; j < N; j++) {
    a_out[j] = 0.0f;
    for (u32 i = 0; i < N; i++) {
      a_out[j] += b_out[j] * xs[N * i + j];
    }
  }
}

static int closeN(float const *got, float const *want) {
  for (u32 i = 0; i < N; i++) {
    float d = (got[i] - want[i]) / want[i];
    if (d < -1e-4f || d > 1e-4f) {
      return 0;
    }
  }
  return 1;
}

int main(void) {
  // A collection at every realize boundary.
  setenv("THVM_GC_KB", "1", 1);
  thvm_init();
  float xs[N * N], want_b[N], want_a[N];
  for (u32 i = 0; i < N * N; i++) {
    xs[i] = 0.125f * (float)(i + 1);
  }
  expect(xs, want_b, want_a);

  // The repro of an optimizer step: out = Bp + gather(H, idx, 0) and
  // loss = sum(out * out), so Bp's grad 2*out is an input of H's grad
  // kernel (the scatter back through the gather) and a root of its own.
  TEST_BEGIN("realize-gc-roots/root-read-by-sibling-root-is-held");
  {
    Term h  = tenRC(9, 4, 0.5f);
    Term bp = tenRC(9, 4, -0.25f);
    Shape s = {0};
    s.ndim    = 2;
    s.dims[0] = 9;
    s.dims[1] = 4;
    u32 itid = tensor_alloc(&CPU_BACKEND, s, DT_INT32);
    int idx[36];
    for (u32 i = 0; i < 36; i++) {
      idx[i] = (int)(i / 4);
    }
    CPU_BACKEND.buf_write(TENS[itid].buf_id, idx, sizeof idx);
    Term out  = uop_binary(UOP_ADD, bp, uop_gather(h, term_new(0, TAG_TEN, DT_INT32, itid), 0));
    u32  ax[2] = {0, 1};
    Term loss = uop_reduce_multi(REDUCE_SUM, 2, ax, uop_binary(UOP_MUL, out, out));
    Shape s0 = {0};
    u32  gtid = tensor_alloc(&CPU_BACKEND, s0, DT_FP32);
    float one = 1.0f;
    CPU_BACKEND.buf_write(TENS[gtid].buf_id, &one, sizeof one);
    u32 ht = (u32)term_val(h), bt = (u32)term_val(bp);
    TENS[ht].requires_grad = 1;
    grad_req_ncount_inc();
    TENS[bt].requires_grad = 1;
    grad_req_ncount_inc();
    wnf(uop_grad(loss, term_new(0, TAG_TEN, DT_FP32, gtid)));
    Term gh = TENS[ht].grad, gb = TENS[bt].grad;
    TENS[ht].requires_grad = 0;
    grad_req_ncount_dec();
    TENS[bt].requires_grad = 0;
    grad_req_ncount_dec();
    CHECK(gh != 0 && gb != 0);
    Term kids[2] = {gh, gb};
    Term r = thvm_realize_many(term_new_ctr(0, kids, 2));
    CHECK(r != 0 && term_tag(r) == TAG_CTR);
    Term rh = term_resolve(term_ctr_at(r, 0)), rb = term_resolve(term_ctr_at(r, 1));
    CHECK(term_tag(rh) == TAG_TEN && term_tag(rb) == TAG_TEN);
    CHECK(buf_live(rh));
    CHECK(buf_live(rb));
    float vh[36], vb[36];
    CPU_BACKEND.buf_read(TENS[(u32)term_val(rh)].buf_id, vh, sizeof vh);
    CPU_BACKEND.buf_read(TENS[(u32)term_val(rb)].buf_id, vb, sizeof vb);
    int ok = 1;
    for (u32 i = 0; i < 36; i++) {
      float want = 2.0f * (0.5f * (float)(i + 1) - 0.25f * (float)(i + 1));
      ok &= vh[i] == want && vb[i] == want;
    }
    CHECK(ok);
    TENS[ht].grad = 0;
    TENS[bt].grad = 0;
  }

  // v = reshape(b) is a root that aliases b's buffer, a boundary of a's
  // pass: the realize returns the alias, which carries b's producer kid.
  TEST_BEGIN("realize-gc-roots/view-of-a-realized-output-is-a-leaf");
  {
    Term x = tenNN();
    Term a, b;
    build(x, &a, &b);
    u32  dn1[2] = {N, 1};
    Term kids[2] = {a, uop_reshape(b, 2, dn1)};
    Term r = thvm_realize_many(term_new_ctr(0, kids, 2));
    CHECK(r != 0);
    Term rv = term_resolve(term_ctr_at(r, 1));
    CHECK(term_tag(rv) == TAG_TEN);
    CHECK_EQ(TENS[(u32)term_val(rv)].producer_kid, 0);
    float got[N];
    readN(rv, got);
    CHECK(closeN(got, want_b));
  }

  // e = expand(reshape(b)) is a strided view of b, which a's pass caches
  // before any kernel fires: the root must wait for b's kernel.
  TEST_BEGIN("realize-gc-roots/strided-view-root-waits-for-its-producer");
  {
    Term x = tenNN();
    Term a, b;
    build(x, &a, &b);
    u32  dn1[2] = {N, 1};
    u32  dnn[2] = {N, N};
    Term kids[2] = {a, uop_expand(uop_reshape(b, 2, dn1), 2, dnn)};
    Term r = thvm_realize_many(term_new_ctr(0, kids, 2));
    CHECK(r != 0);
    Term re = term_resolve(term_ctr_at(r, 1));
    CHECK(term_tag(re) == TAG_TEN);
    float ge[N * N];
    CPU_BACKEND.buf_read(TENS[(u32)term_val(re)].buf_id, ge, sizeof ge);
    int ok = 1;
    for (u32 i = 0; i < N; i++) {
      for (u32 j = 0; j < N; j++) {
        float d = (ge[N * i + j] - want_b[i]) / want_b[i];
        ok &= d > -1e-4f && d < 1e-4f;
      }
    }
    CHECK(ok);
  }

  // Reaching a kernel the kernel GC stripped fails the realize instead of
  // running nothing and handing back whatever the buffer held.
  TEST_BEGIN("realize-gc-roots/firing-a-stripped-kernel-fails-the-realize");
  {
    Term x = tenNN();
    u32  k0 = KERNELS_NEXT;
    Term y  = term_resolve(thvm_realize(uop_binary(UOP_MUL, x, x)));
    CHECK(term_tag(y) == TAG_TEN && KERNELS_NEXT > k0);
    u32 t   = (u32)term_val(y);
    u32 kid = KERNELS_NEXT - 1;
    CHECK_EQ(KERNELS[kid].output_tid, t);
    TENS[t].producer_kid = kid;
    KERNELS[kid].stripped = 1;
    CHECK(thvm_realize(uop_binary(UOP_ADD, y, y)) == 0);
    CHECK(strstr(thvm_realize_error(), "stripped") != NULL);
    KERNELS[kid].stripped = 0;
    TENS[t].producer_kid = 0;
    CHECK(thvm_realize(uop_binary(UOP_ADD, y, y)) != 0);
    CHECK(thvm_realize_error()[0] == '\0');
  }

  TEST_BEGIN("realize-gc-roots/collection-copies-the-unsubstituted-graph");
  {
    Term x = tenNN();
    Term a, b;
    build(x, &a, &b);
    Term c = uop_binary(UOP_MUL, b, b);
    extern_pin_handle_set(77, c);
    u64 gc0 = gc_count();
    Term kids[2] = {a, c};
    CHECK(thvm_realize_many(term_new_ctr(0, kids, 2)) != 0);
    CHECK(gc_count() > gc0);
    Term c2 = extern_pin_handle_get(77);
    CHECK(term_tag(c2) == TAG_UOP && term_ext(c2) == UOP_MUL);
    CHECK(term_val(c2) >= gc_from_start() && term_val(c2) < gc_from_end());
    Term b2 = term_resolve(heap_read(term_val(c2) + 0));
    CHECK(term_tag(b2) == TAG_UOP && term_ext(b2) == UOP_REDUCE);
    Term again = thvm_realize(c2);
    CHECK(term_tag(term_resolve(again)) == TAG_TEN);
    float got[N], want_c[N];
    for (u32 i = 0; i < N; i++) {
      want_c[i] = want_b[i] * want_b[i];
    }
    readN(again, got);
    CHECK(closeN(got, want_c));
    extern_pin_handle_drop(77);
  }

  TEST_BEGIN("realize-gc-roots/tendesc-grad-moves-with-the-heap");
  {
    Term x = tenNN();
    u32  tid = (u32)term_val(x);
    TENS[tid].grad = uop_binary(UOP_ADD, x, x);
    gc_collect(NULL, 0);
    Term g = TENS[tid].grad;
    CHECK(term_tag(g) == TAG_UOP && term_ext(g) == UOP_ADD);
    CHECK(term_val(g) >= gc_from_start() && term_val(g) < gc_from_end());
    CHECK_EQ(heap_read(term_val(g) + 0), x);
    TENS[tid].grad = 0;
  }

  TEST_BEGIN("realize-gc-roots/deferred-collection-relocates-host-roots");
  {
    thvm_gc_defer(1);
    Term x    = tenNN();
    Term lazy = uop_binary(UOP_ADD, x, x);
    u64  gc0  = gc_count();
    Term r    = thvm_realize(uop_binary(UOP_MUL, x, x));
    CHECK(term_tag(term_resolve(r)) == TAG_TEN);
    CHECK(gc_count() == gc0);
    CHECK(thvm_gc_pending());
    Term roots[1] = {lazy};
    thvm_gc_collect_deferred(roots, 1);
    CHECK(gc_count() == gc0 + 1);
    CHECK(!thvm_gc_pending());
    CHECK(term_val(roots[0]) >= gc_from_start() && term_val(roots[0]) < gc_from_end());
    CHECK_EQ(heap_read(term_val(roots[0]) + 0), x);
    thvm_gc_defer(0);
  }

  TEST_REPORT();
}
