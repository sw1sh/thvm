// test_alo_gc.c -- a lazily unfolded def survives a Cheney collection.  An
// unforced ALO names its binders through ALO_STATES (book binder -> the
// instance's LAM cell) and its shared dup cells through DUP_SHARE; both hold
// dyn locs, so the collection must keep those cells and remap the tables.
// Each case collects from a from-space that holds the instance, then
// overwrites what that from-space held.
#include "../src/thvm.c"
#include "test.h"

static void scribble(u64 lo, u64 hi) {
  for (u64 i = lo; i < hi; i++) {
    HEAP[i] = term_new(0, TAG_NUM, DT_INT32, 0xBAD);
  }
}

static void collect_low(Term *roots, u32 n) {
  if (gc_from_start() != 0) {
    gc_collect(roots, n);
  }
  u64 lo = gc_from_start(), hi = HEAP_NEXT;
  gc_collect(roots, n);
  scribble(lo, hi);
}

static Shape shape23(void) {
  Shape s   = {0};
  s.ndim    = 2;
  s.dims[0] = 2;
  s.dims[1] = 3;
  return s;
}

static Term ten6(f32 const *v) {
  u32 tid = tensor_alloc(&CPU_BACKEND, shape23(), DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)v, 6 * sizeof(f32));
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static int read6(Term t, f32 *out) {
  Term r = term_resolve(thvm_realize(t));
  if (term_tag(r) != TAG_TEN) {
    return 0;
  }
  u32 tid = (u32)term_val(r);
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, 6 * sizeof(f32));
  return 1;
}

static Term app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

int main(void) {
  thvm_init();
  f32 const x[6] = { 1, 2, 3, 4, 5, 6 };
  f32 const y[6] = { 10, 20, 30, 40, 50, 60 };

  // @0 := lam v. v + t, unfolded one layer: its body is an unforced ALO whose
  // state binds the book binder to the instance's LAM cell.
  TEST_BEGIN("alo/an-unfolded-lam-applied-after-a-collection");
  {
    Term  t   = ten6(x);
    u64   lam = heap_alloc(1);
    Shape s   = shape23();
    heap_set(lam, uop_binary(UOP_ADD, term_new(0, TAG_VAR, 0, lam), t));
    lam_shape_set(lam, &s);
    thvm_def_register(0, term_new(0, TAG_LAM, 0, lam));
    Term f = alo_realize(DEFS[0], 0);
    CHECK_EQ(term_tag(heap_read(term_val(f))), TAG_ALO);
    collect_low(&f, 1);
    f32 got[6] = { 0 };
    CHECK(read6(app(f, ten6(y)), got));
    for (u32 i = 0; i < 6; i++) {
      CHECK(got[i] == x[i] + y[i]);
    }
  }

  // @1 := lam v. dup a b = v; a + b.  The body and its first projection are
  // unfolded, so the instance's dup cell sits in DUP_SHARE while the second
  // projection is still a book BJ1 under an ALO.
  TEST_BEGIN("alo/a-shared-dup-cell-survives-a-collection");
  {
    u64  lam = heap_alloc(1);
    u64  dup = heap_alloc(1);
    heap_set(dup, term_new(0, TAG_VAR, 0, lam));
    Term a = term_new(0, TAG_DP0, 5, dup);
    Term b = term_new(0, TAG_DP1, 5, dup);
    heap_set(lam, uop_binary(UOP_ADD, a, b));
    Shape s = shape23();
    lam_shape_set(lam, &s);
    thvm_def_register(1, term_new(0, TAG_LAM, 0, lam));
    Term f    = alo_realize(DEFS[1], 0);
    Term body = alo_force(heap_read(term_val(f)));
    (void)alo_force(heap_read(term_val(body) + 0));
    CHECK_EQ(term_tag(heap_read(term_val(body) + 1)), TAG_ALO);
    collect_low(&f, 1);
    f32 got[6] = { 0 };
    CHECK(read6(app(f, ten6(y)), got));
    for (u32 i = 0; i < 6; i++) {
      CHECK(got[i] == 2 * y[i]);
    }
  }

  thvm_free();
  TEST_REPORT();
}
