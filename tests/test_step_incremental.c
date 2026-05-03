// test_step_incremental.c -- per-step cost of the WL stepper path.
//
// Verifies the redex_step_* surface that powers TStepBegin /
// TInteract / TStepEnd:
//
//   1. Correctness vs nf:  fire every redex_step_drain_fresh entry
//      until the worklist is empty, compare the resulting head to
//      what nf produces on the same input.
//   2. Correctness of the "fresh" set:  the union of fresh sets
//      drained across one step session equals (post \ pre)
//      computed by the legacy redex_enumerate oracle.
//   3. Incrementality:  build a small redex chain plus a large pile
//      of unrelated heap cells, attach a session, then fire one
//      redex.  Assert that HOT_HEAP_REPLACE_CELLS bumps by O(uses-
//      of-redex) -- well below HEAP_NEXT -- proving the inserted
//      unrelated cells were NOT visited.
//
// Bound chosen for incrementality: HOT_HEAP_REPLACE_CELLS bump must
// be < HEAP_NEXT/8.  Legacy code adds HEAP_NEXT per heap_replace
// call; the inverse-index path adds the use-list length.

#include "../src/thvm.c"
#include "test.h"

static Term build_id_lam(void) {
  u64  lam_loc = heap_alloc(1);
  Term var     = term_new(0, TAG_VAR, 0, lam_loc);
  heap_set(lam_loc, var);
  return term_new(0, TAG_LAM, 0, lam_loc);
}

static Term build_app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

// Stuff `n_extra` unrelated APP-VAR cells into the heap so HEAP_NEXT
// grows.  None are redexes; nothing references them.  The dead VAR
// keeps the cells live (term_arity > 0) so they survive any GC.
static void pad_heap(u32 n_extra) {
  for (u32 i = 0; i < n_extra; i++) {
    Term v   = term_new(0, TAG_VAR, 0, 0);
    Term era = term_new(0, TAG_ERA, 0, 0);
    (void)build_app(v, era);   // not stored anywhere -- pure padding
  }
}

// True iff `t` appears anywhere in `arr[0..n)`.
static u8 contains(Term *arr, u32 n, Term t) {
  for (u32 i = 0; i < n; i++) if (arr[i] == t) return 1;
  return 0;
}

int main(void) {
  thvm_init();

  // === 1. Correctness: step session reproduces nf ===
  TEST_BEGIN("step/session-fires-to-same-result-as-nf");
  // ((id era_a) era_b) -- two-step reduction: inner APP-LAM, then
  // outer APP-ERA.  Result is ERA.
  Term era_a = term_new(0, TAG_ERA, 0, 0);
  Term era_b = term_new(0, TAG_ERA, 0, 0);
  Term inner = build_app(build_id_lam(), era_a);
  Term outer = build_app(inner, era_b);

  // Oracle: snapshot via nf on a clone.  We can't snapshot nf result
  // and then ALSO drive step on the same heap, so we fork the test
  // by recording outer's stepped-to result and comparing to a fresh
  // build that goes through nf directly.
  Term root_seed[1] = { outer };
  redex_step_attach(root_seed, 1);
  // Drain the seed fresh-set first so the loop just consumes new
  // ones produced by fires.  redex_step_attach pre-set already
  // contains the seeds, so first drain is empty.
  Term drain[64];
  u32 dn = redex_step_drain_fresh(drain, 64);
  CHECK_EQ(dn, 0);
  // Fire the inner first (the only initial redex).
  Term fired = redex_step_fire(inner);
  CHECK(fired != 0);
  // After the fire, outer is freshly promoted to APP-ERA.
  dn = redex_step_drain_fresh(drain, 64);
  CHECK(dn >= 1);
  CHECK(contains(drain, dn, outer));
  // Fire it.
  Term fired2 = redex_step_fire(outer);
  CHECK(fired2 != 0);
  CHECK_EQ(term_tag(fired2), TAG_ERA);
  redex_step_detach();

  // Sanity: nf on a fresh equivalent term reaches an ERA at the head.
  thvm_free(); thvm_init();
  Term era_a2 = term_new(0, TAG_ERA, 0, 0);
  Term era_b2 = term_new(0, TAG_ERA, 0, 0);
  Term inner2 = build_app(build_id_lam(), era_a2);
  Term outer2 = build_app(inner2, era_b2);
  Term nf_res = nf(outer2);
  CHECK_EQ(term_tag(nf_res), TAG_ERA);

  // === 2. "fresh" set matches the legacy enumerate-based oracle ===
  TEST_BEGIN("step/fresh-set-equals-redex-enumerate-oracle");
  thvm_free(); thvm_init();
  Term ea1 = term_new(0, TAG_ERA, 0, 0);
  Term ea2 = term_new(0, TAG_ERA, 0, 0);
  Term ai = build_app(build_id_lam(), ea1);
  Term ao = build_app(ai, ea2);

  // Oracle pre.
  Term pre[64];
  Term roots3[1] = { ao };
  u32  pre_n = redex_enumerate(roots3, 1, pre, 64);

  // Step session: attach, fire ai, drain fresh.  Note the session
  // pre-set should match `pre` (modulo set semantics).
  redex_step_attach(roots3, 1);
  // Initial drain is empty (everything in attach went to pre-set).
  Term d0[64]; u32 d0_n = redex_step_drain_fresh(d0, 64);
  CHECK_EQ(d0_n, 0);
  (void)redex_step_fire(ai);
  Term fresh_step[64];
  u32  fresh_step_n = redex_step_drain_fresh(fresh_step, 64);

  // Oracle post + symmetric diff.
  Term post[64];
  Term roots4[1] = { ao };
  u32  post_n = redex_enumerate(roots4, 1, post, 64);

  // Build oracle fresh = post \ pre.
  Term oracle_fresh[64];
  u32  oracle_fresh_n = 0;
  for (u32 i = 0; i < post_n; i++) {
    if (!contains(pre, pre_n, post[i])) {
      if (oracle_fresh_n < 64) oracle_fresh[oracle_fresh_n++] = post[i];
    }
  }
  // Oracle and step set should agree as sets (order-independent).
  CHECK_EQ(fresh_step_n, oracle_fresh_n);
  for (u32 i = 0; i < oracle_fresh_n; i++) {
    CHECK(contains(fresh_step, fresh_step_n, oracle_fresh[i]));
  }
  redex_step_detach();

  // === 3. Incrementality: large heap, single fire is O(local) ===
  TEST_BEGIN("step/fire-cost-is-independent-of-heap-size");
  thvm_free(); thvm_init();
  // Build a small fixed redex (one APP-LAM) shared in two parent
  // slots, then pad the heap.
  Term era_x  = term_new(0, TAG_ERA, 0, 0);
  Term redex5 = build_app(build_id_lam(), era_x);
  // Stash it as the fun of two parent APPs so heap_replace has work
  // (3 cells to patch: two parent fun-slots + the original outer
  // build_app's fun-slot).
  Term era_y  = term_new(0, TAG_ERA, 0, 0);
  Term era_z  = term_new(0, TAG_ERA, 0, 0);
  Term parent_a = build_app(redex5, era_y);
  Term parent_b = build_app(redex5, era_z);
  (void)parent_a; (void)parent_b;

  // Pad with N unrelated cells.  N chosen so HEAP_NEXT > 8 * uses;
  // 4096 padding APPs = 8192 cells of extras vs ~8 cells used by
  // the redex graph.
  u64 hb_before_pad = HEAP_NEXT;
  pad_heap(4096);
  u64 heap_size = HEAP_NEXT;
  CHECK(heap_size > hb_before_pad + 4096);

  // Attach the session AFTER the pad is in place: we want one
  // attach to be allowed (it walks the heap once to build the
  // inverse index) but the FIRE itself must not re-walk the heap.
  Term roots5[2] = { parent_a, parent_b };
  redex_step_attach(roots5, 2);

  // Reset hot counters AFTER attach to isolate per-fire cost.
  hot_counters_reset();
  Term fired5 = redex_step_fire(redex5);
  CHECK(fired5 != 0);

  // The fire allocated some cells (e.g. the body's substitution
  // chains) but the heap_replace walk should have visited only the
  // use-list of `redex5` (3 cells: outer build_app's fun + the
  // two parent fun-slots).  Without the inverse index the legacy
  // path would have added HEAP_NEXT (>8000) per call; assert we're
  // strictly below HEAP_NEXT/8 to leave room for fire-internal
  // helper allocations.
  u64 cells_visited = HOT_HEAP_REPLACE_CELLS;
  u64 enum_cells    = HOT_REDEX_ENUM_CELLS;
  CHECK(cells_visited < heap_size / 8);
  // No re-enumerate during a step fire either.
  CHECK_EQ(enum_cells, (u64)0);

  redex_step_detach();

  thvm_free();
  TEST_REPORT();
}
