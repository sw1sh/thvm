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

// Walk the bufferize graph and the realize table together.  Every
// realized REALIZE_INFO entry must appear in the graph with
// realized=1; an unrealized entry that is in the graph must have
// realized=0 and a removed_by stamp.  bufferize_realized_count must
// equal the number of realized REALIZE_INFO entries.
static void check_graph_matches_realize_info(void) {
  u32 realized = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo const *info = &REALIZE_INFO[i];
    u32 idx = bufferize_find_by_loc(info->loc);
    if (info->realized) {
      realized++;
      CHECK(idx != 0xFFFFFFFFu);
      if (idx == 0xFFFFFFFFu) continue;
      BBufferize const *b = bufferize_buffer_at(idx);
      CHECK(b != NULL);
      CHECK_EQ(b->loc, info->loc);
      CHECK_EQ(b->op, info->op);
      CHECK_EQ(b->consumer_count, info->consumer_count);
      CHECK_EQ(b->buffer_id, idx + 1);
      CHECK_EQ(b->realized, 1);
    } else if (idx != 0xFFFFFFFFu) {
      BBufferize const *b = bufferize_buffer_at(idx);
      CHECK_EQ(b->realized, 0);
      CHECK(b->removed_by != NULL);
    }
  }
  CHECK_EQ(bufferize_realized_count(), realized);
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

  TEST_BEGIN("bufferize/inline-constants-stamps-removed-by");
  // A multi-consumer CONST gets MULTI seeded and then unmarked by
  // the inline-constants rule.  After classify the bufferize graph
  // must keep the CONST as a record with realized=0 and
  // removed_by="inline-constants".
  Term k  = uop_const(DT_FP32, 0x3F800000u);   // 1.0f
  Term ka = uop_binary(UOP_ADD, k, a);
  Term kb = uop_binary(UOP_ADD, k, b);
  Term kr = uop_binary(UOP_ADD, ka, kb);
  realize_classify(kr);
  u32 k_idx = bufferize_find_by_loc(term_val(k));
  CHECK(k_idx != 0xFFFFFFFFu);
  if (k_idx != 0xFFFFFFFFu) {
    BBufferize const *kb2 = bufferize_buffer_at(k_idx);
    CHECK_EQ(kb2->realized, 0);
    CHECK(kb2->removed_by != NULL);
    if (kb2->removed_by != NULL) {
      CHECK_EQ(strcmp(kb2->removed_by, "inline-constants"), 0);
    }
    CHECK(kb2->reasons & BUFFERIZE_REASON_MULTI);
  }
  // Total buffer count includes the removed const; realized count
  // does not.  And the realized count must equal the live boundary
  // set seen by realize_is_realized.
  u32 live = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    if (REALIZE_INFO[i].realized) live++;
  }
  CHECK_EQ(bufferize_realized_count(), live);
  CHECK(bufferize_buffer_count() > bufferize_realized_count());

  TEST_BEGIN("bufferize/realized-count-matches-realize-info");
  // Build a small reduce graph and confirm realized_count tracks
  // REALIZE_INFO across the whole rewrite pass.
  Term red2 = uop_reduce(REDUCE_SUM, 0, a);
  realize_classify(red2);
  u32 live2 = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    if (REALIZE_INFO[i].realized) live2++;
  }
  CHECK_EQ(bufferize_realized_count(), live2);

  TEST_BEGIN("bufferize/current-rule-resets-after-apply");
  // Outside realize_rewrite_apply the current rule pointer is NULL
  // again.  Otherwise downstream callers would see a stale rule.
  CHECK_EQ(bufferize_current_rule(), (char const *)NULL);

  TEST_BEGIN("bufferize/edges-multi-consumer-no-movement");
  // shared = a + b; left = shared * c; right = shared * a;
  // root = left + right.  The shared buffer feeds the root through
  // two independent paths, neither of which goes through a movement
  // op, so the index table should record two source=shared,
  // consumer=root edges with chain_len=0.
  Term sh   = uop_binary(UOP_ADD, a, b);
  Term lf   = uop_binary(UOP_MUL, sh, c);
  Term rt   = uop_binary(UOP_MUL, sh, a);
  Term tot  = uop_binary(UOP_ADD, lf, rt);
  realize_classify(tot);
  u32 sh_idx = bufferize_find_by_loc(term_val(sh));
  u32 tot_idx = bufferize_find_by_loc(term_val(tot));
  CHECK(sh_idx != 0xFFFFFFFFu);
  CHECK(tot_idx != 0xFFFFFFFFu);
  if (sh_idx != 0xFFFFFFFFu && tot_idx != 0xFFFFFFFFu) {
    u32 sh_id  = bufferize_buffer_at(sh_idx)->buffer_id;
    u32 tot_id = bufferize_buffer_at(tot_idx)->buffer_id;
    u32 edges = 0;
    for (u32 i = 0; i < bufferize_index_count(); i++) {
      BIndex const *e = bufferize_index_at(i);
      if (e->source_buffer_id == sh_id && e->consumer_buffer_id == tot_id) {
        edges++;
        CHECK_EQ(e->movement_chain_len, 0);
        CHECK_EQ(e->has_reshape, 0);
        CHECK_EQ(e->has_permute, 0);
      }
    }
    CHECK_EQ(edges, 2);
  }

  TEST_BEGIN("bufferize/edges-record-reshape-on-movement-chain");
  // sh2 = a + b (shape {3}) is consumed twice: once directly by a
  // NEG (no movement on that edge) and once through a RESHAPE to
  // {3,1}.  The two parents make sh2 multi-consumer and therefore a
  // realized buffer.  Combining both branches into a {3,1} root
  // gives us one edge with has_reshape=1 from the movement path
  // and one with chain_len=0 from the direct path.
  u32 dims31[2] = {3, 1};
  Term sh2        = uop_binary(UOP_ADD, a, b);
  Term branch_lin = uop_unary(UOP_NEG, sh2);
  Term branch_rs  = uop_reshape(sh2, 2, dims31);
  Term branch_lin_rs = uop_reshape(branch_lin, 2, dims31);
  Term root3      = uop_binary(UOP_ADD, branch_lin_rs, branch_rs);
  realize_classify(root3);
  u32 sh2_idx   = bufferize_find_by_loc(term_val(sh2));
  u32 root3_idx = bufferize_find_by_loc(term_val(root3));
  CHECK(sh2_idx != 0xFFFFFFFFu);
  CHECK(root3_idx != 0xFFFFFFFFu);
  if (sh2_idx != 0xFFFFFFFFu && root3_idx != 0xFFFFFFFFu) {
    u32 sh2_id   = bufferize_buffer_at(sh2_idx)->buffer_id;
    u32 root3_id = bufferize_buffer_at(root3_idx)->buffer_id;
    u32 edges = 0;
    u32 with_reshape = 0;
    for (u32 i = 0; i < bufferize_index_count(); i++) {
      BIndex const *e = bufferize_index_at(i);
      if (e->source_buffer_id == sh2_id
          && e->consumer_buffer_id == root3_id) {
        edges++;
        if (e->has_reshape) with_reshape++;
      }
    }
    CHECK_EQ(edges, 2);
    CHECK(with_reshape >= 1);
  }

  TEST_BEGIN("bufferize/indexes-for-consumer-helper");
  // bufferize_indexes_for_consumer should return the same set of
  // indices as a manual scan.  Use the shared/root3 graph above.
  u32 buf[16];
  u32 n = bufferize_indexes_for_consumer(
      bufferize_buffer_at(root3_idx)->buffer_id, buf, 16);
  CHECK(n >= 1);
  for (u32 i = 0; i < n; i++) {
    BIndex const *e = bufferize_index_at(buf[i]);
    CHECK(e != NULL);
    CHECK_EQ(e->consumer_buffer_id,
             bufferize_buffer_at(root3_idx)->buffer_id);
  }
  // Calling with cap=0 still returns the count.
  u32 n0 = bufferize_indexes_for_consumer(
      bufferize_buffer_at(root3_idx)->buffer_id, NULL, 0);
  CHECK_EQ(n0, n);

  thvm_free();
  TEST_REPORT();
}
