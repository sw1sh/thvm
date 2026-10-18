// test_book_def_gc.c -- a def survives a Cheney collection.  The snapshot
// copies every node into the book, so no book cell holds a dyn loc; the
// collector never moves a book term, and an unforced ALO keeps its book cell.
// Each case collects from a from-space that holds the low locs the book uses,
// then overwrites what that from-space held.
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

int main(void) {
  thvm_init();
  f32 const x[6] = { 1, 2, 3, 4, 5, 6 };

  // A movement node nested in an ADD: the snapshot used to keep it as a dyn
  // term in a book cell, which no collection reached.
  TEST_BEGIN("book/nested-movement-node-survives-a-collection");
  {
    u32  d6[1] = { 6 };
    Term rsh   = uop_reshape(ten6(x), 1, d6);
    thvm_def_register(0, uop_binary(UOP_ADD, rsh, rsh));
    collect_low(NULL, 0);
    f32 got[6] = { 0 };
    CHECK(read6(term_new_ref(0), got));
    for (u32 i = 0; i < 6; i++) {
      CHECK(got[i] == 2 * x[i]);
    }
  }

  // @1 := lam v. v * t, applied after a collection.
  TEST_BEGIN("book/lam-def-survives-a-collection");
  {
    Term  t   = ten6(x);
    u64   lam = heap_alloc(1);
    Shape s   = shape23();
    heap_set(lam, uop_binary(UOP_MUL, term_new(0, TAG_VAR, 0, lam), t));
    lam_shape_set(lam, &s);
    thvm_def_register(1, term_new(0, TAG_LAM, 0, lam));
    collect_low(NULL, 0);
    u64 app = heap_alloc(2);
    heap_set(app + 0, term_new_ref(1));
    heap_set(app + 1, ten6(x));
    f32 got[6] = { 0 };
    CHECK(read6(term_new(0, TAG_APP, 0, app), got));
    for (u32 i = 0; i < 6; i++) {
      CHECK(got[i] == x[i] * x[i]);
    }
  }

  // One layer of @2 := (t + t) * t: the MUL's first cell is an ALO over the
  // book ADD, live across the collection.
  TEST_BEGIN("book/unforced-alo-keeps-its-book-term");
  {
    Term t = ten6(x);
    thvm_def_register(2, uop_binary(UOP_MUL, uop_binary(UOP_ADD, t, t), t));
    Term m = alo_realize(DEFS[2], 0);
    CHECK_EQ(term_tag(heap_read(term_val(m))), TAG_ALO);
    collect_low(&m, 1);
    f32 got[6] = { 0 };
    CHECK(read6(m, got));
    for (u32 i = 0; i < 6; i++) {
      CHECK(got[i] == 2 * x[i] * x[i]);
    }
  }

  // An ADD over a VAR whose binder APP-LAM already filled: the def holds the
  // value, not a free variable.
  TEST_BEGIN("book/substituted-var-snapshots-its-value");
  {
    u64  lam = heap_alloc(1);
    Term v   = term_new(0, TAG_VAR, 0, lam);
    heap_subst_var(lam, ten6(x));
    thvm_def_register(3, uop_binary(UOP_ADD, v, v));
    collect_low(NULL, 0);
    f32 got[6] = { 0 };
    CHECK(read6(term_new_ref(3), got));
    for (u32 i = 0; i < 6; i++) {
      CHECK(got[i] == 2 * x[i]);
    }
  }

  thvm_free();
  TEST_REPORT();
}
