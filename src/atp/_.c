// thvm_atp_* - saturation loop state (stage 5.1).
//
// Heap-allocated AtpState plus init / free / add_equation / set_goal
// helpers.  The actual saturation step (thvm_atp_step) lands in 5.2;
// the priority-aware CP selection in 5.3; the recursive-descent
// rewriter feeding step 4 of the algorithm in 5.4.  This file just
// gives the loop a place to live.
//
// See docs/plans/saturation_loop.md for the algorithm.

fn AtpState *thvm_atp_init(const KboConfig *cfg, u32 step_cap) {
  AtpState *s = (AtpState *)calloc(1, sizeof(AtpState));
  if (s == NULL) return NULL;
  s->kbo      = cfg;
  s->step_cap = step_cap;
  return s;
}

fn void thvm_atp_free(AtpState *s) {
  if (s == NULL) return;
  free(s);
}

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Returns 1 on success, 0 if the
// queue is full.
fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || s->n_cps >= ATP_MAX_CPS) return 0;
  s->cp_lhs[s->n_cps] = lhs;
  s->cp_rhs[s->n_cps] = rhs;
  s->n_cps++;
  return 1;
}

// Set the conjecture (single equation goal_lhs == goal_rhs).
// Calling with goal_lhs == 0 clears the goal (completion mode).
fn void thvm_atp_set_goal(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return;
  s->goal_lhs = lhs;
  s->goal_rhs = rhs;
}

// Pop the front CP (FIFO).  Shift the rest of the queue down by
// one slot to keep the array dense.  Returns 1 on success, 0 if
// the queue was empty.  Stage 5.3 will replace this with a
// priority-collapse selection over INC-wrapped CPs.
fn u8 thvm_atp_select_cp(AtpState *s, Term *lhs_out, Term *rhs_out) {
  if (s == NULL || s->n_cps == 0) return 0;
  *lhs_out = s->cp_lhs[0];
  *rhs_out = s->cp_rhs[0];
  for (u32 i = 1; i < s->n_cps; i++) {
    s->cp_lhs[i - 1] = s->cp_lhs[i];
    s->cp_rhs[i - 1] = s->cp_rhs[i];
  }
  s->n_cps--;
  return 1;
}

// Push one rule onto R; returns 1 on success, 0 if R is full.
static u8 atp_push_rule(AtpState *s, Term lhs, Term rhs) {
  if (s->n_rules >= ATP_MAX_RULES) return 0;
  s->lhs[s->n_rules] = lhs;
  s->rhs[s->n_rules] = rhs;
  s->n_rules++;
  return 1;
}

// One full saturation step.  See docs/plans/saturation_loop.md
// sec.2 for the algorithm.  Order:
//   1. goal_check   -- cheap; may close if a prior step proved
//   2. step_cap     -- TIMEOUT if exceeded
//   3. select_cp    -- QUEUE_EMPTY if exhausted
//   4. normalize    -- both sides under current R (NORM_CAP = 64)
//   5. trivialize   -- skip if sides become kbo_eq
//   6. orient + add -- KBO + unfailing fallback
//   7. interreduce  -- drop subsumed older rules
//   8. generate_cps -- (new x R) + (old x new), adjusted for
//                      dropped old rules
//   9. goal_check   -- may close after new rule(s) integrated
//  10. step++       -- only on a "real" step that didn't close
//
// Returns one of: ATP_RUNNING (continue), ATP_PROVED (goal hit),
// ATP_TIMEOUT (step cap), ATP_QUEUE_EMPTY (saturation reached
// without proving the goal).
fn AtpStatus thvm_atp_step(AtpState *s) {
  if (s == NULL) return ATP_QUEUE_EMPTY;

  AtpStatus goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  if (s->step >= s->step_cap) return ATP_TIMEOUT;

  Term cp_lhs = 0, cp_rhs = 0;
  if (!thvm_atp_select_cp(s, &cp_lhs, &cp_rhs)) {
    return ATP_QUEUE_EMPTY;
  }

  const u32 NORM_CAP = 64;
  Term l = thvm_rewrite_normalize(cp_lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  Term r = thvm_rewrite_normalize(cp_rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);

  if (kbo_eq(l, r)) {
    s->step++;
    return ATP_RUNNING;
  }

  AtpAddedRange added = thvm_atp_orient_and_add(s, l, r);
  if (added.count == 0) {
    // R full, or some other refusal.  Count the work and continue.
    s->step++;
    return ATP_RUNNING;
  }

  // Interreduce shifts new-rule indices down by `dropped`.
  u32 dropped = thvm_atp_interreduce(s, added);
  AtpAddedRange post = added;
  post.first = (dropped > post.first) ? 0 : (post.first - dropped);

  thvm_atp_generate_cps(s, post);

  goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  s->step++;
  return ATP_RUNNING;
}

// Drive thvm_atp_step until it returns a non-RUNNING status.
fn AtpStatus thvm_atp_run(AtpState *s) {
  AtpStatus st;
  do {
    st = thvm_atp_step(s);
  } while (st == ATP_RUNNING);
  return st;
}

// Goal check: normalize both sides of the conjecture under the
// current R; if they're now structurally equal, the goal is
// proved.  Returns ATP_PROVED on a hit, ATP_RUNNING otherwise.
// Skips cleanly (returns ATP_RUNNING) when no goal is set
// (goal_lhs == 0) -- the completion-mode case.
//
// Top-only rewriting today via thvm_rewrite_normalize; stage 5.4's
// recursive descent will widen coverage to sub-positions.
//
// Step cap NORM_CAP = 64 bounds the normalization (matches the
// ballpark used in tests/test_rewrite.c's headline demo); tune
// once we have benchmark data.
fn AtpStatus thvm_atp_goal_check(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0) return ATP_RUNNING;
  const u32 NORM_CAP = 64;
  Term l = thvm_rewrite_normalize(s->goal_lhs, s->lhs, s->rhs,
                                  s->n_rules, NORM_CAP);
  Term r = thvm_rewrite_normalize(s->goal_rhs, s->lhs, s->rhs,
                                  s->n_rules, NORM_CAP);
  return kbo_eq(l, r) ? ATP_PROVED : ATP_RUNNING;
}

// Walk the older rules (indices [0, added.first)) and drop any
// whose LHS reduces under the freshly-added rule(s).  Each dropped
// rule's simplified equation goes back onto the CP queue so the
// saturation loop has a chance to re-orient it under the smaller R.
//
// Today this uses the top-position-only `thvm_rewrite_normalize`;
// stage 5.4's recursive-descent rewriter will automatically widen
// the coverage to sub-positions without changing this function.
//
// Returns the number of older rules that were dropped.
fn u32 thvm_atp_interreduce(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0 || added.first == 0) return 0;

  // Copy the new rules' Terms by value so we can safely compact the
  // R array beneath them.  Term is 64-bit; the heap cells they point
  // to don't move.
  Term new_lhs[2];
  Term new_rhs[2];
  u32  n_new = added.count;
  if (n_new > 2) n_new = 2;
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k] = s->lhs[added.first + k];
    new_rhs[k] = s->rhs[added.first + k];
  }

  u32 dropped = 0;
  u32 i       = 0;
  while (i < added.first - dropped) {
    Term old_lhs = s->lhs[i];
    Term old_rhs = s->rhs[i];
    Term reduced = thvm_rewrite_normalize(old_lhs, new_lhs, new_rhs, n_new, 16);
    if (!kbo_eq(reduced, old_lhs)) {
      // The older rule's LHS simplified -- drop it and requeue
      // (reduced, old_rhs) for re-orientation.
      thvm_atp_add_equation(s, reduced, old_rhs);
      for (u32 j = i + 1; j < s->n_rules; j++) {
        s->lhs[j - 1] = s->lhs[j];
        s->rhs[j - 1] = s->rhs[j];
      }
      s->n_rules--;
      dropped++;
      // Don't increment i; the next older rule shifted down to slot i.
    } else {
      i++;
    }
  }
  return dropped;
}

// Generate fresh CPs from the freshly-added rules `added` against
// the current rule set R, push survivors onto the CP queue.
// Drops overflow silently (queue cap or temp-buffer cap).  Returns
// the number of CPs successfully pushed.
//
// To avoid recomputing CPs already in the queue, the enumeration
// is restricted to (new x all_R) + (old x new), where the old x
// old slice is exactly the work we already did before the add.
//
// Temp buffer sized for one batch; large rule sets may produce
// more CPs than fit and silently drop them (matches Waldmeister's
// drop-on-overflow policy in `KPVerwaltung.c` -- *Kritische-Paare-
// Verwaltung*, "critical-pair management").

#define ATP_CP_BATCH 1024

fn u32 thvm_atp_generate_cps(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0) return 0;

  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 nbuf = 0;

  // (new x all_R): new rule on the outside.
  nbuf = thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                   first, last, 0, n,
                                   buf, ATP_CP_BATCH);
  // (old x new): old rule on the outside, new rule fed as inner.
  // Skip (new x new) -- already covered above.
  if (first > 0 && nbuf < ATP_CP_BATCH) {
    nbuf += thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                      0, first, first, last,
                                      buf + nbuf, ATP_CP_BATCH - nbuf);
  }

  u32 pushed = 0;
  for (u32 i = 0; i < nbuf; i++) {
    if (s->n_cps >= ATP_MAX_CPS) break;
    s->cp_lhs[s->n_cps] = buf[i].lhs;
    s->cp_rhs[s->n_cps] = buf[i].rhs;
    s->n_cps++;
    pushed++;
  }
  return pushed;
}

// Orient via KBO and push the rule(s).  See header comment for the
// dispatch table.  Atomic: if the unfailing fallback can't fit both
// orientations, neither is added.
fn AtpAddedRange thvm_atp_orient_and_add(AtpState *s, Term lhs, Term rhs) {
  AtpAddedRange r = {0, 0};
  if (s == NULL) return r;

  KboCmp c = thvm_kbo(lhs, rhs, s->kbo);
  switch (c) {
    case KBO_GT: {
      u32 idx = s->n_rules;
      if (atp_push_rule(s, lhs, rhs)) { r.first = idx; r.count = 1; }
      return r;
    }
    case KBO_LT: {
      u32 idx = s->n_rules;
      if (atp_push_rule(s, rhs, lhs)) { r.first = idx; r.count = 1; }
      return r;
    }
    case KBO_UN: {
      // Unfailing fallback: need 2 slots for atomicity.
      if (s->n_rules + 2 > ATP_MAX_RULES) return r;
      u32 idx = s->n_rules;
      atp_push_rule(s, lhs, rhs);
      atp_push_rule(s, rhs, lhs);
      r.first = idx;
      r.count = 2;
      return r;
    }
    case KBO_EQ:
    default:
      return r;
  }
}
