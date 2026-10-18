// test_realize_many_war.c - realize_many orders its roots like tinygrad's
// create_schedule (tinygrad/schedule/__init__.py:32-83): a root reading a
// buffer's prior state runs before another root's ASSIGN overwrites that
// buffer, whatever the CTR order, and roots that each read what the other
// overwrites fail with a recoverable "cycle" error instead of exiting.
// The ordering walk runs on every multi-root realize, so it must not keep
// memory in proportion to the heap once it returns.
#include "../src/thvm.c"
#include "test.h"

static Term mk_assign(Term dst, Term src) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, dst);
  heap_set(loc + 1, src);
  return term_new(0, TAG_UOP, UOP_ASSIGN, loc);
}

static Term ten4(float v) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = 4;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  float data[4] = {v, v, v, v};
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, sizeof data);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static float first(Term t) {
  float data[4] = {0};
  CPU_BACKEND.buf_read(TENS[(u32)term_val(term_resolve(t))].buf_id, data, sizeof data);
  return data[0];
}

static Term realize2(Term a, Term b) {
  Term kids[2] = {a, b};
  return thvm_realize_many(term_new_ctr(0, kids, 2));
}

int main(void) {
  thvm_init();

  // test_assign.py:318-326: y = x + 10 reads x before x := (x * 2) + 3.
  for (int swap = 0; swap < 2; swap++) {
    TEST_BEGIN(swap ? "realize-many-war/corealize-y-first" : "realize-many-war/corealize-x-first");
    Term x  = ten4(1.0f);
    Term y  = uop_binary(UOP_ADD, x, ten4(10.0f));
    Term a1 = mk_assign(x, uop_binary(UOP_MUL, x, ten4(2.0f)));
    Term a2 = mk_assign(a1, uop_binary(UOP_ADD, a1, ten4(3.0f)));
    Term r  = swap ? realize2(y, a2) : realize2(a2, y);
    CHECK(r != 0);
    CHECK(term_tag(r) == TAG_CTR);
    CHECK(first(term_ctr_at(r, swap ? 0 : 1)) == 11.0f);
    CHECK(first(x) == 5.0f);
    CHECK(thvm_realize_error()[0] == '\0');
  }

  // test_assign.py:268-277: a += b and b += (a + 9) read each other's
  // prior state; tinygrad raises "cycle detected in assign graph".
  TEST_BEGIN("realize-many-war/crossunder-cycle-is-recoverable");
  {
    Term a = ten4(2.0f), b = ten4(3.0f);
    Term c  = uop_binary(UOP_ADD, a, ten4(9.0f));
    Term aa = mk_assign(a, uop_binary(UOP_ADD, a, b));
    Term bb = mk_assign(b, uop_binary(UOP_ADD, b, c));
    Term kids[2] = {aa, bb};
    Term ctr = term_new_ctr(0, kids, 2);
    CHECK(thvm_realize_many(ctr) == 0);
    CHECK(strstr(thvm_realize_error(), "cycle") != NULL);
    CHECK(thvm_realize(ctr) == 0);
    CHECK(strstr(thvm_realize_error(), "cycle") != NULL);
    CHECK(first(a) == 2.0f && first(b) == 3.0f);
  }

  TEST_BEGIN("realize-many-war/realize-after-cycle");
  {
    Term p = ten4(4.0f), q = ten4(1.0f);
    Term r = realize2(mk_assign(p, uop_binary(UOP_ADD, p, q)), q);
    CHECK(r != 0);
    CHECK(thvm_realize_error()[0] == '\0');
    CHECK(first(p) == 5.0f);
  }

  // test_assign.py:234-241: b += a after a += b is RAW only, no cycle.
  TEST_BEGIN("realize-many-war/crossover");
  {
    Term a  = ten4(2.0f), b = ten4(3.0f);
    Term aa = mk_assign(a, uop_binary(UOP_ADD, a, b));
    Term bb = mk_assign(b, uop_binary(UOP_ADD, b, aa));
    CHECK(realize2(aa, bb) != 0);
    CHECK(first(a) == 5.0f);
    CHECK(first(b) == 8.0f);
  }

  // More roots than the 256 the walk's root sets used to hold: 150
  // assigns first in CTR order, then 150 reads of the states they
  // supersede.  Every root is realized and every read sees the prior state.
  TEST_BEGIN("realize-many-war/bundle-of-300-roots");
  {
    enum { K = 150 };
    Term xs[K], kids[2 * K];
    for (u32 k = 0; k < K; k++) {
      xs[k] = ten4(1.0f + (float)k);
      Term a1 = mk_assign(xs[k], uop_binary(UOP_MUL, xs[k], ten4(2.0f)));
      kids[k]     = mk_assign(a1, uop_binary(UOP_ADD, a1, ten4(3.0f)));
      kids[K + k] = uop_binary(UOP_ADD, xs[k], ten4(10.0f));
    }
    Term r = thvm_realize_many(term_new_ctr(0, kids, 2 * K));
    CHECK(r != 0 && term_tag(r) == TAG_CTR && term_ctr_n(r) == 2 * K);
    u32 bad = 0;
    for (u32 k = 0; k < K && r != 0 && term_ctr_n(r) == 2 * K; k++) {
      float v = 1.0f + (float)k;
      Term  y = term_resolve(term_ctr_at(r, K + k));
      if (term_tag(y) != TAG_TEN || first(y) != v + 10.0f) {
        bad++;
      }
      if (first(xs[k]) != 2.0f * v + 3.0f) {
        bad++;
      }
    }
    CHECK_EQ(bad, 0u);
  }

  // 1024 nodes 4096 cells apart, reached by the ASSIGN pre-check (root 0
  // has none) and by the per-root walks (root 1 ends in one): a u32 side
  // table indexed by heap cell would keep one page per node (>= 4 MiB)
  // resident after the walk.
  TEST_BEGIN("realize-many-war/walk-keeps-no-heap-sized-memory");
  {
    Term leaf[2]  = {ten4(1.0f), ten4(2.0f)};
    Term roots[2] = {leaf[0], leaf[1]};
    for (u32 r = 0; r < 2; r++) {
      for (u32 i = 0; i < 512; i++) {
        heap_alloc(4096);
        roots[r] = uop_binary(UOP_ADD, roots[r], leaf[r]);
      }
    }
    roots[1] = mk_assign(leaf[1], roots[1]);
    u32 order[2], wave_end[2];
    u8  has_assign = 0;
    long long rss0 = thvm_atp_live_rss_bytes();
    CHECK(realize_waves(roots, 2, order, wave_end, &has_assign) == 1);
    long long rss1 = thvm_atp_live_rss_bytes();
    CHECK(has_assign == 1);
    CHECK(rss0 > 0 && rss1 - rss0 < (2LL << 20));
  }

  TEST_REPORT();
}
