// test_bufferize.c - Phase 0 of docs/plans/bufferize.md.
//
// realize_classify still owns boundary selection; bufferize_build
// projects the result into an explicit graph.  These tests confirm
// the projection is faithful: every realized loc has exactly one
// B_BUFFERIZE record with matching reasons and consumer count, the
// realize root has a B_STORE pointing at it, and inlined locs do
// not show up.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

// Walk the bufferize graph and the realize table together; every
// realized REALIZE_INFO entry must appear exactly once with matching
// op/consumer count, and no inlined entry may appear.
static void check_graph_matches_realize_info(void) {
  u32 realized = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo const *info = &REALIZE_INFO[i];
    if (info->realized) {
      realized++;
      u32 idx = bufferize_find_by_loc(info->loc);
      CHECK(idx != 0xFFFFFFFFu);
      if (idx == 0xFFFFFFFFu) continue;
      BBufferize const *b = bufferize_buffer_at(idx);
      CHECK(b != NULL);
      CHECK_EQ(b->loc, info->loc);
      CHECK_EQ(b->op, info->op);
      CHECK_EQ(b->consumer_count, info->consumer_count);
      CHECK_EQ(b->buffer_id, idx + 1);
    } else {
      CHECK_EQ(bufferize_find_by_loc(info->loc), 0xFFFFFFFFu);
    }
  }
  CHECK_EQ(bufferize_buffer_count(), realized);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("bufferize/single-uop-root");
  // ADD over two TENs.  Just the root realizes; the graph should
  // have one buffer with the ROOT reason and one store.
  u32 ta = alloc_f32_tensor(3);
  u32 tb = alloc_f32_tensor(3);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term add = uop_binary(UOP_ADD, a, b);
  realize_classify(add);
  CHECK_EQ(bufferize_buffer_count(), 1);
  CHECK_EQ(bufferize_store_count(), 1);
  BBufferize const *bf = bufferize_buffer_at(0);
  CHECK(bf != NULL);
  CHECK_EQ(bf->buffer_id, 1);
  CHECK_EQ(bf->loc, term_val(add));
  CHECK_EQ(bf->is_root, 1);
  CHECK(bf->reasons & BUFFERIZE_REASON_ROOT);
  BStore const *st = bufferize_store_at(0);
  CHECK(st != NULL);
  CHECK_EQ(st->buffer_id, bf->buffer_id);
  CHECK_EQ(st->loc, term_val(add));
  check_graph_matches_realize_info();

  TEST_BEGIN("bufferize/chain-only-root");
  // (a + b) * c -- the inner ADD is single-consumer and inlined,
  // so the bufferize graph holds only the MUL root.
  u32 tc = alloc_f32_tensor(3);
  Term c = term_new(0, TAG_TEN, DT_FP32, tc);
  Term add2 = uop_binary(UOP_ADD, a, b);
  Term mul2 = uop_binary(UOP_MUL, add2, c);
  realize_classify(mul2);
  CHECK_EQ(bufferize_buffer_count(), 1);
  CHECK_EQ(bufferize_store_count(), 1);
  CHECK_EQ(bufferize_find_by_loc(term_val(add2)), 0xFFFFFFFFu);
  u32 mul_idx = bufferize_find_by_loc(term_val(mul2));
  CHECK(mul_idx != 0xFFFFFFFFu);
  if (mul_idx != 0xFFFFFFFFu) {
    BBufferize const *m = bufferize_buffer_at(mul_idx);
    CHECK(m->reasons & BUFFERIZE_REASON_ROOT);
    CHECK_EQ(m->is_root, 1);
  }
  check_graph_matches_realize_info();

  TEST_BEGIN("bufferize/multi-consumer-fanout");
  // shared = a + b; left = shared * c; right = shared * a;
  // root = left + right.  shared has 2 distinct UOp parents so it
  // gets MULTI; the root gets ROOT.
  Term shared = uop_binary(UOP_ADD, a, b);
  Term left   = uop_binary(UOP_MUL, shared, c);
  Term right  = uop_binary(UOP_MUL, shared, a);
  Term root   = uop_binary(UOP_ADD, left, right);
  realize_classify(root);
  // shared may or may not survive the remove-removable-bufferize
  // pass depending on env; either way, the graph must mirror
  // REALIZE_INFO exactly.
  check_graph_matches_realize_info();
  // The root buffer must always exist with the ROOT reason.
  u32 root_idx = bufferize_find_by_loc(term_val(root));
  CHECK(root_idx != 0xFFFFFFFFu);
  if (root_idx != 0xFFFFFFFFu) {
    BBufferize const *r = bufferize_buffer_at(root_idx);
    CHECK_EQ(r->is_root, 1);
    CHECK(r->reasons & BUFFERIZE_REASON_ROOT);
  }
  // Exactly one store points at the realize root.
  CHECK_EQ(bufferize_store_count(), 1);
  BStore const *root_store = bufferize_store_at(0);
  CHECK_EQ(root_store->loc, term_val(root));

  TEST_BEGIN("bufferize/buffer-ids-are-stable-and-dense");
  // Buffer ids are 1..N in insertion order, no gaps.
  for (u32 i = 0; i < bufferize_buffer_count(); i++) {
    BBufferize const *bb = bufferize_buffer_at(i);
    CHECK(bb != NULL);
    CHECK_EQ(bb->buffer_id, i + 1);
  }
  CHECK_EQ(bufferize_buffer_at(bufferize_buffer_count()), (BBufferize *)NULL);

  TEST_BEGIN("bufferize/non-uop-root-empty-graph");
  // Building from a non-UOp root yields an empty graph and no store.
  realize_classify(a);   // a is a TAG_TEN, classify bails early
  CHECK_EQ(bufferize_buffer_count(), 0);
  CHECK_EQ(bufferize_store_count(), 0);

  TEST_BEGIN("bufferize/reduce-projects-reduce-reason");
  // REDUCE always seeds REALIZE_REASON_REDUCE, which projects to
  // BUFFERIZE_REASON_REDUCE.  Build SUM(a) and check the bit.
  Term red = uop_reduce(REDUCE_SUM, 0, a);
  realize_classify(red);
  u32 red_idx = bufferize_find_by_loc(term_val(red));
  CHECK(red_idx != 0xFFFFFFFFu);
  if (red_idx != 0xFFFFFFFFu) {
    BBufferize const *rb = bufferize_buffer_at(red_idx);
    CHECK(rb->reasons & BUFFERIZE_REASON_REDUCE);
    CHECK(rb->reasons & BUFFERIZE_REASON_ROOT);
  }
  check_graph_matches_realize_info();

  thvm_free();
  TEST_REPORT();
}
