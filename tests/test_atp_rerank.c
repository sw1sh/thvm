// test_atp_rerank.c - live CP-queue re-rank seam.
//
// A WL-side scorer (a GNN) re-ranks the critical-pair (CP) queue
// mid-saturation.  This exercises the three C seams that expose the
// queue and let the scorer permute selection order:
//   - thvm_atp_queued_cp_count : how many CPs are live in the queue,
//   - thvm_atp_queued_cps      : pull (lhs, rhs, seq) for each live CP,
//   - thvm_atp_set_cp_pri_by_seq : re-key priorities by seq + re-heapify.
//
// Re-ranking only PERMUTES selection (no CP added / dropped), so it
// cannot affect completeness.  The asserts here prove the re-key + Floyd
// build-heap actually changes which CP the engine selects next, and that
// a no-op re-key (every CP keeps its current priority) leaves the next
// selection unchanged.

#include "../src/thvm.c"
#include "test.h"

#include <stdio.h>

#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_i(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_i, cs, 1); }

// Same KBO config the headline group-axiom proofs use (weights i=0,
// f=1, e=1, a=1; precedence i > f > e > a; var weight 1).
static u32 gw[5] = {0, 1, 0, 1, 1};
static u32 gp[5] = {0, 2, 4, 3, 1};
static const KboConfig GROUP_CFG = {
  .weights = gw, .precedence = gp, .n_labels = 5, .var_weight = 1,
};

static void load_group_axioms(AtpState *s) {
  thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),           mk_v(VAR_x));
  thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
  thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                        mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
}

// Drive the saturation a bounded number of steps so the CP heap fills
// with several overlaps, stopping early if the proof closes or the
// queue is consumed.  Returns with the engine paused mid-saturation.
static void fill_queue(AtpState *s, u32 max_steps) {
  for (u32 k = 0; k < max_steps; k++) {
    if (thvm_atp_queued_cp_count(s) > 4u) break;
    AtpStatus st = thvm_atp_step(s);
    if (st != ATP_RUNNING) break;
  }
}

int main(void) {
  thvm_init();

  // === Snapshot the live queue ========================================
  // After a few steps the heap holds several CPs; queued_cp_count agrees
  // with queued_cps, and every snapshotted (lhs, rhs) is a well-formed
  // (non-zero) Term pair.
  TEST_BEGIN("atp/rerank/snapshot-live-queue");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 256);
    // Completion mode (no goal) so stepping never closes early; just
    // builds up the queue.
    load_group_axioms(s);
    fill_queue(s, 64u);

    u32 n = thvm_atp_queued_cp_count(s);
    CHECK(n > 1u);

    Term lhs[64], rhs[64];
    u32  seq[64];
    u32 got = thvm_atp_queued_cps(s, lhs, rhs, seq, 64u);
    CHECK_EQ(got, n);
    for (u32 i = 0; i < got; i++) {
      CHECK(lhs[i] != 0);
      CHECK(rhs[i] != 0);
    }
    // seqs are distinct (the CP insertion ids are unique per CP).
    for (u32 i = 0; i < got; i++) {
      for (u32 j = i + 1u; j < got; j++) CHECK(seq[i] != seq[j]);
    }
    // cap truncates: ask for 2, get exactly 2 (when n >= 2).
    if (n >= 2u) {
      Term l2[2], r2[2];
      u32 s2[2];
      CHECK_EQ(thvm_atp_queued_cps(s, l2, r2, s2, 2u), 2u);
    }
    // NULL out columns are skipped (seq-only pull still returns the count).
    CHECK_EQ(thvm_atp_queued_cps(s, NULL, NULL, seq, 64u), n);

    thvm_atp_free(s);
  }

  // === Re-key changes the next selection ==============================
  // Peek which seq the engine would pop next (the heap min, recovered by
  // a non-mutating select on a clone-equivalent state), then promote a
  // DIFFERENT queued CP to priority 0 and confirm it pops first.
  TEST_BEGIN("atp/rerank/rekey-changes-selection");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 256);
    load_group_axioms(s);
    fill_queue(s, 64u);

    u32 n = thvm_atp_queued_cp_count(s);
    CHECK(n > 1u);

    Term lhs[64], rhs[64];
    u32  seq[64];
    u32 got = thvm_atp_queued_cps(s, lhs, rhs, seq, 64u);
    CHECK_EQ(got, n);

    // The heap root (slot 0) is the CP the next select_cp would pop on
    // the default weight branch.  Capture its seq.
    u32 default_seq = s->cp_seq[0];

    // Pick a DIFFERENT queued CP (the live slot whose seq != default).
    u32 target_seq = default_seq;
    for (u32 i = 0; i < got; i++) {
      if (seq[i] != default_seq) { target_seq = seq[i]; break; }
    }
    CHECK(target_seq != default_seq);

    // Re-key: give the target priority 0 and every other CP a high
    // priority, so the target is the unambiguous heap min.
    u32 new_seq[64], new_pri[64];
    for (u32 i = 0; i < got; i++) {
      new_seq[i] = seq[i];
      new_pri[i] = (seq[i] == target_seq) ? 0u : 1000000u;
    }
    thvm_atp_set_cp_pri_by_seq(s, new_seq, new_pri, got);

    // Re-rank must not add or drop CPs.
    CHECK_EQ(thvm_atp_queued_cp_count(s), n);
    // The promoted CP is now the heap root.
    CHECK_EQ(s->cp_seq[0], target_seq);
    // The trace id of the CP the next select WILL pop (slot 0).
    u32 expect_trace = s->cp_trace[0];

    // Force the deterministic weight branch (heap root) for this assert:
    // the periodic FIFO pick fires at cp_select_count % 11 == 10, which
    // would otherwise pick the oldest CP rather than the heap min.
    s->cp_select_count = 0u;

    // select actually pops it (default weight branch picks slot 0).
    Term pl = 0, pr = 0;
    u8 ok = thvm_atp_select_cp(s, &pl, &pr);
    CHECK(ok);
    CHECK(pl != 0 && pr != 0);
    // The popped CP is the one we promoted, NOT the default heap min.
    CHECK_EQ(s->last_popped_trace, expect_trace);

    thvm_atp_free(s);
  }

  // === No-op re-key leaves the next selection unchanged ===============
  // Setting every CP's priority to its CURRENT cp_pri must not change
  // which CP is the heap root -- the re-heapify is a stable identity on
  // an already-valid heap.
  TEST_BEGIN("atp/rerank/noop-rekey-stable");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 256);
    load_group_axioms(s);
    fill_queue(s, 64u);

    u32 n = thvm_atp_queued_cp_count(s);
    CHECK(n > 1u);

    // Snapshot each live slot's current (seq, pri).
    u32 seq[64], pri[64];
    for (u32 i = 0; i < n; i++) {
      seq[i] = s->cp_seq[i];
      pri[i] = s->cp_pri[i];
    }
    u32 root_seq_before = s->cp_seq[0];

    thvm_atp_set_cp_pri_by_seq(s, seq, pri, n);

    CHECK_EQ(thvm_atp_queued_cp_count(s), n);
    CHECK_EQ(s->cp_seq[0], root_seq_before);
    // Force the weight branch so "next selection unchanged" compares the
    // heap root, not the periodic FIFO oldest-CP pick.
    u32 expect_trace = s->cp_trace[0];
    s->cp_select_count = 0u;
    // The next selected CP is the same one as before the no-op re-key.
    Term pl = 0, pr = 0;
    u8 ok = thvm_atp_select_cp(s, &pl, &pr);
    CHECK(ok);
    CHECK_EQ(s->last_popped_trace, expect_trace);

    thvm_atp_free(s);
  }

  // === Unknown / popped seqs are ignored ==============================
  // Re-keying with a seq that is not in the live queue is a safe no-op
  // for that entry (no crash, no spurious change).
  TEST_BEGIN("atp/rerank/unknown-seq-ignored");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 256);
    load_group_axioms(s);
    fill_queue(s, 64u);

    u32 n = thvm_atp_queued_cp_count(s);
    CHECK(n > 1u);
    u32 root_seq_before = s->cp_seq[0];

    // A seq guaranteed not to be live (cp_seq_next is the next-to-assign
    // id, so it is larger than any live seq).
    u32 bogus_seq = s->cp_seq_next + 100u;
    u32 bogus_pri = 0u;
    thvm_atp_set_cp_pri_by_seq(s, &bogus_seq, &bogus_pri, 1u);

    CHECK_EQ(thvm_atp_queued_cp_count(s), n);
    CHECK_EQ(s->cp_seq[0], root_seq_before);
    thvm_atp_free(s);
  }

  thvm_free();
  TEST_REPORT();
}
