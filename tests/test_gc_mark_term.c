// test_gc_mark_term.c - the preserve mark reaches every buffer a root holds,
// through any node and through a def's book template, and skips orphans.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32(u32 n) {
  Shape s = {0}; s.ndim = 1; s.dims[0] = n;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static int preserved(u32 tid) {
  return CPU_BUFS[TENS[tid].buf_id].preserved;
}

// Marks from `root` alone and reports whether tid's buffer was marked.
static int marks(Term root, u32 tid) {
  cpu_buf_clear_preserved(0);
  CHECK(gc_mark_terms(&root, 1));
  return preserved(tid);
}

static Term node(u8 tag, u32 ext, Term const *cells, u32 n) {
  u64 loc = heap_alloc(n);
  for (u32 i = 0; i < n; i++) {
    heap_set(loc + i, cells[i]);
  }
  return term_new(0, tag, ext, loc);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("gc-mark/leaf-tag-ten-marks-its-buf");
  u32 tA = alloc_f32(4);
  CHECK(marks(ten(tA), tA));

  TEST_BEGIN("gc-mark/uop-add-marks-both-children-not-an-orphan");
  u32  a = alloc_f32(4);
  u32  b = alloc_f32(4);
  u32  c = alloc_f32(4);
  Term ab[2] = { ten(a), ten(b) };
  Term add = node(TAG_UOP, UOP_ADD, ab, 2);
  cpu_buf_clear_preserved(0);
  CHECK(gc_mark_terms(&add, 1));
  CHECK_EQ(preserved(a), 1);
  CHECK_EQ(preserved(b), 1);
  CHECK_EQ(preserved(c), 0);

  TEST_BEGIN("gc-mark/atoms-and-zero-mark-nothing");
  u32  d = alloc_f32(4);
  Term atoms[4] = { 0, term_new(0, TAG_NUM, DT_FP32, 42), term_new(0, TAG_VAR, 0, 0),
                    term_new(0, TAG_ERA, 0, 0) };
  cpu_buf_clear_preserved(0);
  CHECK(gc_mark_terms(atoms, 4));
  CHECK_EQ(preserved(d), 0);

  TEST_BEGIN("gc-mark/a-cycle-ends");
  u64 cyc_loc = heap_alloc(2);
  heap_set(cyc_loc + 0, term_new(0, TAG_ERA, 0, 0));
  heap_set(cyc_loc + 1, term_new(0, TAG_APP, 0, cyc_loc));
  Term cyc = term_new(0, TAG_APP, 0, cyc_loc);
  CHECK(gc_mark_terms(&cyc, 1));

  // Shapes the mark used to stop at: a buffer behind each one survives.
  TEST_BEGIN("gc-mark/through-plain-dup-projections");
  {
    u32  t    = alloc_f32(4);
    Term body = ten(t);
    Term dp0  = node(TAG_DP0, 7, &body, 1);
    CHECK(marks(dp0, t));
    CHECK(marks(term_new(0, TAG_DP1, 7, term_val(dp0)), t));
  }

  TEST_BEGIN("gc-mark/through-a-grad-projection");
  {
    u32  t     = alloc_f32(4);
    Term cells[3] = { term_new(0, TAG_ERA, 0, 0), ten(t), term_new(0, TAG_ERA, 0, 0) };
    CHECK(marks(node(TAG_DP1, DUP_GRAD_FLAG, cells, 3), t));
  }

  TEST_BEGIN("gc-mark/through-a-ctr");
  {
    u32  t     = alloc_f32(4);
    Term kids[2] = { term_new(0, TAG_NUM, DT_INT32, 0), ten(t) };
    CHECK(marks(term_new_ctr(0, kids, 2), t));
  }

  TEST_BEGIN("gc-mark/through-a-pri-argument");
  {
    u32  t     = alloc_f32(4);
    Term cells[2] = { term_new(0, TAG_NUM, DT_INT32, 1), ten(t) };
    CHECK(marks(node(TAG_PRI, 0, cells, 2), t));
  }

  TEST_BEGIN("gc-mark/through-eql-and-dsu");
  {
    u32 t = alloc_f32(4);
    CHECK(marks(term_new_eql(term_new(0, TAG_ERA, 0, 0), ten(t)), t));
    u32 s = alloc_f32(4);
    CHECK(marks(term_new_dsu(term_new(0, TAG_NUM, DT_INT32, 1), ten(s), term_new(0, TAG_ERA, 0, 0)), s));
  }

  TEST_BEGIN("gc-mark/through-a-store");
  {
    u32  t     = alloc_f32(4);
    Term cells[3] = { term_new(0, TAG_ERA, 0, 0), term_new(0, TAG_ERA, 0, 0), ten(t) };
    CHECK(marks(node(TAG_UOP, UOP_STORE, cells, 3), t));
  }

  TEST_BEGIN("gc-mark/through-a-kernel-output");
  {
    u32  t     = alloc_f32(4);
    Term cells[2] = { ten(t), term_new(0, TAG_NUM, DT_INT32, 0) };
    CHECK(marks(node(TAG_UOP, UOP_KERNEL, cells, 2), t));
  }

  // A REF names its def in ext; the def is a book template.
  TEST_BEGIN("gc-mark/follows-a-ref-into-its-book-def");
  {
    u32  e  = alloc_f32(4);
    u32  f  = alloc_f32(4);
    Term ef[2] = { ten(e), ten(f) };
    thvm_def_register(3, node(TAG_UOP, UOP_MUL, ef, 2));
    cpu_buf_clear_preserved(0);
    Term ref = term_new_ref(3);
    CHECK(gc_mark_terms(&ref, 1));
    CHECK_EQ(preserved(e), 1);
    CHECK_EQ(preserved(f), 1);
  }

  // One layer of @4 := (g + h) * k: the MUL's cell 0 is an ALO over the book
  // ADD.  The mark reads it in the book and forces nothing.
  TEST_BEGIN("gc-mark/reads-an-unforced-alo-in-the-book");
  {
    u32  g  = alloc_f32(4);
    u32  h  = alloc_f32(4);
    u32  k  = alloc_f32(4);
    Term gh[2]  = { ten(g), ten(h) };
    Term ghk[2] = { node(TAG_UOP, UOP_ADD, gh, 2), ten(k) };
    thvm_def_register(4, node(TAG_UOP, UOP_MUL, ghk, 2));
    Term m = alo_realize(DEFS[4], 0);
    CHECK_EQ(term_tag(heap_read(term_val(m))), TAG_ALO);
    u64 next = HEAP_NEXT;
    cpu_buf_clear_preserved(0);
    CHECK(gc_mark_terms(&m, 1));
    CHECK_EQ(preserved(g), 1);
    CHECK_EQ(preserved(h), 1);
    CHECK_EQ(preserved(k), 1);
    CHECK_EQ(HEAP_NEXT, next);
  }

  // The rollback keeps a buffer this realize allocated when a pinned term
  // reaches it through a dup projection, the shape a backward graph shares a
  // forward activation through.
  TEST_BEGIN("gc-mark/rollback-keeps-a-buffer-a-pinned-dup-holds");
  {
    u32  wm   = cpu_buf_pool_begin();
    u32  t    = alloc_f32(4);
    u32  buf  = TENS[t].buf_id;
    Term body = ten(t);
    Term dp1  = node(TAG_DP1, 9, &body, 1);
    extern_pin_term(dp1);
    CHECK(mark_gc_preserve_all_roots(0));
    cpu_buf_pool_rollback_with_preserve(wm);
    CHECK(cpu_buf_refcount(buf) > 0);
    extern_unpin_term(dp1);
    cpu_buf_clear_preserved(0);
  }

  thvm_free();
  TEST_REPORT();
}
