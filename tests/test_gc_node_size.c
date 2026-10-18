// test_gc_node_size.c -- a Cheney collection moves every live node whole.
//
// gc_node_size tells the evacuator how many cells a node owns; a tag it has no
// case for is treated as an atom and left in from-space.  Each case roots one
// node, or one wnf frame through WNF_LAST_STACK, collects, overwrites the
// from-space it left, and reads the moved cells.
#include "../src/thvm.c"
#include "test.h"

static Term num(u32 v) {
  return term_new(0, TAG_NUM, DT_INT32, v);
}

static void scribble(u64 lo, u64 hi) {
  for (u64 i = lo; i < hi; i++) {
    HEAP[i] = num(0xBAD);
  }
}

// Collect with `t` as the only root (frame = 0) or as the only WNF_LAST_STACK
// entry (frame = 1); returns the moved term.
static Term collect_one(Term t, int frame) {
  u64  lo       = gc_from_start();
  u64  hi       = HEAP_NEXT;
  Term roots[1] = { t };
  if (frame) {
    WNF_LAST_STACK[0]  = t;
    WNF_LAST_STACK_LEN = 1;
    gc_collect(NULL, 0);
    t = WNF_LAST_STACK[0];
    WNF_LAST_STACK_LEN = 0;
  } else {
    gc_collect(roots, 1);
    t = roots[0];
  }
  scribble(lo, hi);
  return t;
}

static int cells_are(Term t, u32 n, u32 first) {
  for (u32 i = 0; i < n; i++) {
    if (HEAP[term_val(t) + i] != num(first + i)) {
      return 0;
    }
  }
  return 1;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("gc-node-size/dsu-moves-whole");
  Term dsu = collect_one(term_new_dsu(num(1), num(2), num(3)), 0);
  CHECK_EQ(term_tag(dsu), TAG_DSU);
  CHECK(cells_are(dsu, 3, 1));

  TEST_BEGIN("gc-node-size/ddu-moves-whole");
  Term ddu = collect_one(term_new_ddu(num(4), num(5), num(6)), 0);
  CHECK_EQ(term_tag(ddu), TAG_DDU);
  CHECK(cells_are(ddu, 3, 4));

  TEST_BEGIN("gc-node-size/dsu-label-frame-moves-its-cell");
  Term d2  = term_new_dsu(num(7), num(8), num(9));
  Term fdl = collect_one(term_new(0, TAG_F_DSU_LAB, 0, term_val(d2)), 1);
  CHECK_EQ(term_tag(fdl), TAG_F_DSU_LAB);
  CHECK(cells_are(fdl, 3, 7));

  TEST_BEGIN("gc-node-size/op2-num-frame-moves-its-cell");
  Term op2 = term_new_op2(OP_ADD, num(10), num(11));
  Term fop = collect_one(term_new(0, TAG_F_OP2_NUM, OP_ADD, term_val(op2)), 1);
  CHECK_EQ(term_tag(fop), TAG_F_OP2_NUM);
  CHECK_EQ(term_ext(fop), OP_ADD);
  CHECK(cells_are(fop, 2, 10));

  TEST_BEGIN("gc-node-size/uop-child-frame-sizes-its-parent");
  Term add = uop_binary(UOP_ADD, term_new(0, TAG_TEN, DT_FP32, 1),
                        term_new(0, TAG_TEN, DT_FP32, 2));
  u32  ext = 1u | ((u32)UOP_ADD << 8);
  Term fuc = collect_one(term_new(0, TAG_F_UOP_CHILD, ext, term_val(add)), 1);
  CHECK_EQ(term_tag(fuc), TAG_F_UOP_CHILD);
  CHECK_EQ(term_ext(fuc), ext);
  CHECK_EQ(HEAP[term_val(fuc) + 0], term_new(0, TAG_TEN, DT_FP32, 1));
  CHECK_EQ(HEAP[term_val(fuc) + 1], term_new(0, TAG_TEN, DT_FP32, 2));

  // Loc 0 reads as "none" (an unbound book binder, assign_fire_claim), so no
  // node may live there: not the first allocation, nor the first cell a
  // collection evacuates into the lower semi-space.
  TEST_BEGIN("gc/loc-0-is-never-a-node");
  gc_init(thvm_heap_cells() / 2);
  Term first   = term_new_op2(OP_ADD, num(1), num(2));
  Term held[1] = { first };
  CHECK(term_val(first) != 0);
  gc_collect(held, 1);
  gc_collect(held, 1);
  CHECK_EQ(gc_from_start(), 0);
  CHECK(term_val(held[0]) != 0);
  CHECK(cells_are(held[0], 2, 1));

  thvm_free();
  TEST_REPORT();
}
