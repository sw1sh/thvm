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
  // Trace-index slots default to ATP_TRACE_NONE (calloc gives 0,
  // which is a valid trace index, so we explicitly fill).
  for (u32 i = 0; i < ATP_MAX_RULES; i++) s->r_trace[i]  = ATP_TRACE_NONE;
  for (u32 i = 0; i < ATP_MAX_CPS;   i++) s->cp_trace[i] = ATP_TRACE_NONE;
  return s;
}

fn void thvm_atp_free(AtpState *s) {
  if (s == NULL) return;
  free(s);
}

// Push a trace entry as a TAG_CTR with label = reason and children
// [NUM(parent_a), NUM(parent_b), lhs, rhs].  Returns the entry's
// index in s->trace, or ATP_TRACE_NONE if the buffer is full.
//
// 6.1b/c will wire this into add_equation / orient_and_add /
// generate_cps; for 6.1a the helper just exists, and the storage is
// init'd to zero by thvm_atp_init's calloc.
static u32 atp_trace_push(AtpState *s, u32 reason, u32 p_a, u32 p_b,
                          Term lhs, Term rhs) {
  if (s == NULL || s->n_trace >= ATP_MAX_TRACE) return ATP_TRACE_NONE;
  Term children[4] = {
    term_new(0, TAG_NUM, 0, p_a),
    term_new(0, TAG_NUM, 0, p_b),
    lhs,
    rhs,
  };
  s->trace[s->n_trace] = term_new_ctr(reason, children, 4);
  u32 idx = s->n_trace;
  s->n_trace++;
  return idx;
}

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Also records a TRACE_AXIOM
// entry so the proof trace (stage 6.1) can identify this CP's
// origin downstream.  Returns 1 on success, 0 if the queue is full.
fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || s->n_cps >= ATP_MAX_CPS) return 0;
  u32 trace_idx = atp_trace_push(s, TRACE_AXIOM,
                                 ATP_TRACE_NONE, ATP_TRACE_NONE,
                                 lhs, rhs);
  s->cp_lhs[s->n_cps]   = lhs;
  s->cp_rhs[s->n_cps]   = rhs;
  s->cp_trace[s->n_cps] = trace_idx;
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

// Total symbol count: TAG_FVR / atoms count as 1; TAG_CTR counts
// itself + the symbols of its children.  This is the "size" used
// by Waldmeister's `--add` heuristic in `ClasHeuristics.c`
// ("classification heuristics") -- the simplest CP-priority
// function: cheapest-by-size wins.
static u32 atp_symbol_count(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      u32 c = 1;
      for (u32 i = 0; i < n; i++) {
        c += atp_symbol_count(term_ctr_at(t, i));
      }
      return c;
    }
    default: return 1;   // FVR / atoms / NUM / etc.
  }
}

// Pop the cheapest CP from the queue, where "cheap" = lowest
// total symbol count across (lhs + rhs) -- the `--add` heuristic.
//
// IC-side encoding (per docs/plans/saturation_loop.md sec.3):
//   each CP becomes  INC^k (CTR_label=idx [lhs, rhs])  where
//   k = symbol_count(lhs) + symbol_count(rhs).  All wrappings are
//   folded into a SUP tree and run through thvm_collapse_ordered;
//   the cheapest leaf comes out first, its CTR label decodes back
//   to the original queue index, and we pop that index.
//
// Singleton case skips the SUP/INC plumbing for speed.  Returns
// 1 on success (out-params populated), 0 on empty queue or any
// decoding failure (defensive).
fn u8 thvm_atp_select_cp(AtpState *s, Term *lhs_out, Term *rhs_out) {
  if (s == NULL || s->n_cps == 0) return 0;
  if (s->n_cps == 1) {
    *lhs_out = s->cp_lhs[0];
    *rhs_out = s->cp_rhs[0];
    s->last_popped_trace = s->cp_trace[0];
    s->n_cps = 0;
    return 1;
  }

  // Build wrapped[i] = INC^k_i(CTR_label=i([lhs_i, rhs_i])).
  Term wrapped[ATP_MAX_CPS];
  for (u32 i = 0; i < s->n_cps; i++) {
    u32 k = atp_symbol_count(s->cp_lhs[i]) + atp_symbol_count(s->cp_rhs[i]);
    Term children[2] = { s->cp_lhs[i], s->cp_rhs[i] };
    Term w = term_new_ctr(i, children, 2);
    for (u32 j = 0; j < k; j++) w = term_new_inc(w);
    wrapped[i] = w;
  }

  // Fold into SUP-tree: SUP(w_0, SUP(w_1, ..., w_{n-1})).  The SUP
  // labels don't matter for collapse_ordered (just structural
  // recursion); use 0.
  Term sup = wrapped[s->n_cps - 1];
  for (u32 i = s->n_cps - 1; i > 0; ) {
    i--;
    u64 loc = heap_alloc(2);
    heap_set(loc + 0, wrapped[i]);
    heap_set(loc + 1, sup);
    sup = term_new(0, TAG_SUP, 0, loc);
  }

  // Collapse, sorted by INC depth ascending.
  Term out[ATP_MAX_CPS];
  u64 n_out = thvm_collapse_ordered(sup, out, (u64)s->n_cps);
  if (n_out == 0) return 0;

  Term first = out[0];
  if (term_tag(first) != TAG_CTR) return 0;
  u32 idx = term_ext(first);
  if (idx >= s->n_cps) return 0;

  *lhs_out = s->cp_lhs[idx];
  *rhs_out = s->cp_rhs[idx];
  s->last_popped_trace = s->cp_trace[idx];
  for (u32 j = idx + 1; j < s->n_cps; j++) {
    s->cp_lhs[j - 1]   = s->cp_lhs[j];
    s->cp_rhs[j - 1]   = s->cp_rhs[j];
    s->cp_trace[j - 1] = s->cp_trace[j];
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

  u32 src_trace = s->last_popped_trace;
  AtpAddedRange added = thvm_atp_orient_and_add(s, l, r);
  if (added.count == 0) {
    // R full, or some other refusal.  Count the work and continue.
    s->step++;
    return ATP_RUNNING;
  }

  // Trace each newly-added rule with its source CP as parent_a.
  // For unfailing 2-way fallback both directions get separate
  // entries so PCL output can identify each rule individually.
  // Stash the trace index in r_trace[] so generate_cps can
  // record TRACE_CP parents for any CP born from this rule.
  for (u32 k = 0; k < added.count; k++) {
    Term rl = s->lhs[added.first + k];
    Term rr = s->rhs[added.first + k];
    u32  t  = atp_trace_push(s, TRACE_ORIENT, src_trace,
                             ATP_TRACE_NONE, rl, rr);
    s->r_trace[added.first + k] = t;
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

// === stage 6.2: PCL-shaped trace serializer ===========================
//
// Walks the trace[] array and emits human-readable text in the shape
// of Waldmeister's PCL ("Proof Construction Language") output.  Each
// line:
//
//   <idx> (<reason> [from <p_a>[, <p_b>]]): <lhs> = <rhs>
//
// Term printer handles TAG_CTR (as "C<lab>(args...)"), TAG_FVR (as
// "x_<id>"), TAG_NUM (as "#<val>"), TAG_ERA (as "ERA"), with a
// "?T<tag>" fallback for any other tag.  Truncates silently on
// buffer overflow.

static u32 atp_pretty_term(Term t, char *buf, u32 cap);

static u32 atp_pretty_ctr(Term t, char *buf, u32 cap) {
  if (cap <= 1) return 0;
  u32 lab = term_ext(t);
  u32 n   = term_ctr_n(t);
  int n_w = snprintf(buf, cap, "C%u", lab);
  if (n_w < 0) return 0;
  u32 w = (u32)n_w;
  if (w >= cap) return cap - 1;
  if (n == 0) return w;
  if (w + 1 >= cap) return w;
  w += (u32)snprintf(buf + w, cap - w, "(");
  for (u32 i = 0; i < n; i++) {
    if (w + 2 >= cap) break;
    if (i > 0) w += (u32)snprintf(buf + w, cap - w, ", ");
    if (w >= cap) return cap - 1;
    w += atp_pretty_term(term_ctr_at(t, i), buf + w, cap - w);
    if (w >= cap) return cap - 1;
  }
  if (w + 1 < cap) w += (u32)snprintf(buf + w, cap - w, ")");
  return w;
}

static u32 atp_pretty_term(Term t, char *buf, u32 cap) {
  if (cap == 0) return 0;
  switch (term_tag(t)) {
    case TAG_FVR: return (u32)snprintf(buf, cap, "x_%u", term_ext(t));
    case TAG_NUM: return (u32)snprintf(buf, cap, "#%u", (u32)term_val(t));
    case TAG_ERA: return (u32)snprintf(buf, cap, "ERA");
    case TAG_CTR: return atp_pretty_ctr(t, buf, cap);
    default:      return (u32)snprintf(buf, cap, "?T%u", term_tag(t));
  }
}

fn u32 thvm_atp_trace_serialize(const AtpState *s, char *buf, u32 cap) {
  if (s == NULL || buf == NULL || cap == 0) return 0;
  buf[0] = '\0';
  u32 w = 0;
  for (u32 i = 0; i < s->n_trace; i++) {
    if (w + 1 >= cap) break;
    Term entry  = s->trace[i];
    u32  reason = term_ext(entry);
    u32  p_a    = (u32)term_val(term_ctr_at(entry, 0));
    u32  p_b    = (u32)term_val(term_ctr_at(entry, 1));
    Term lhs    = term_ctr_at(entry, 2);
    Term rhs    = term_ctr_at(entry, 3);

    const char *type_str = "?";
    switch (reason) {
      case TRACE_AXIOM:  type_str = "axiom";  break;
      case TRACE_ORIENT: type_str = "orient"; break;
      case TRACE_CP:     type_str = "cp";     break;
    }

    int n;
    if (p_a == ATP_TRACE_NONE) {
      n = snprintf(buf + w, cap - w, "%u (%s): ", i, type_str);
    } else if (p_b == ATP_TRACE_NONE) {
      n = snprintf(buf + w, cap - w, "%u (%s from %u): ", i, type_str, p_a);
    } else {
      n = snprintf(buf + w, cap - w, "%u (%s from %u, %u): ", i, type_str,
                   p_a, p_b);
    }
    if (n < 0) break;
    w += (u32)n;
    if (w + 1 >= cap) break;

    w += atp_pretty_term(lhs, buf + w, cap - w);
    if (w + 4 >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, " = ");

    w += atp_pretty_term(rhs, buf + w, cap - w);
    if (w + 1 >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, "\n");
  }
  if (w >= cap) w = cap - 1;
  buf[w] = '\0';
  return w;
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
        s->lhs[j - 1]     = s->lhs[j];
        s->rhs[j - 1]     = s->rhs[j];
        s->r_trace[j - 1] = s->r_trace[j];
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

// Stage 7.1: trivial-joinability check.  Normalize both sides of a
// candidate CP under the current rule set R and compare; if they
// collapse to the same term, the CP is joinable-by-R and adds no
// new equational consequences -- it can be discarded.
//
// This is the Waldmeister `Grundzusammenfuehrung` ("ground-merging")
// criterion at its weakest, equivalent to Twee's "joinable-by-current-
// R" pruning.  Stronger variants (ground-joinability over a sample
// of substitutions, AC-aware joinability) are deferred to 7.2+.
//
// Cost: two `thvm_rewrite_normalize` calls per CP candidate.  Worth
// it when the saturation produces many joinable CPs (group axioms
// generate ~hundreds of trivially-joinable overlaps per round).
static u8 atp_cp_trivially_joinable(AtpState *s, Term lhs, Term rhs) {
  const u32 NORM_CAP = 64;
  Term l = thvm_rewrite_normalize(lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  Term r = thvm_rewrite_normalize(rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  return kbo_eq(l, r);
}

// Helper: push a batch of CPs onto the queue with TRACE_CP entries
// pointing at the two source rules' trace indices.  Drops overflow
// silently.  CPs that are trivially joinable under current R are
// dropped early (stage 7.1) and counted in `n_cps_dropped_joinable`.
// Returns the count of CPs successfully pushed.
static u32 atp_push_cps_traced(AtpState *s, const CriticalPair *cps,
                               u32 ncps, u32 parent_a, u32 parent_b) {
  u32 pushed = 0;
  for (u32 i = 0; i < ncps; i++) {
    if (s->n_cps >= ATP_MAX_CPS) break;
    if (atp_cp_trivially_joinable(s, cps[i].lhs, cps[i].rhs)) {
      s->n_cps_dropped_joinable++;
      continue;
    }
    u32 t = atp_trace_push(s, TRACE_CP, parent_a, parent_b,
                           cps[i].lhs, cps[i].rhs);
    s->cp_lhs[s->n_cps]   = cps[i].lhs;
    s->cp_rhs[s->n_cps]   = cps[i].rhs;
    s->cp_trace[s->n_cps] = t;
    s->n_cps++;
    pushed++;
  }
  return pushed;
}

fn u32 thvm_atp_generate_cps(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0) return 0;

  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;

  // (new x all_R): the new rule is i (outer), j ranges over all
  // existing rules (including the new ones for new x new self-overlap).
  for (u32 i = first; i < last; i++) {
    for (u32 j = 0; j < n; j++) {
      if (s->n_cps >= ATP_MAX_CPS) break;
      u32 nbuf = thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                           i, i + 1, j, j + 1,
                                           buf, ATP_CP_BATCH);
      pushed += atp_push_cps_traced(s, buf, nbuf,
                                    s->r_trace[i], s->r_trace[j]);
    }
  }

  // (old x new): old rule on the outside, new rule fed as inner.
  for (u32 i = 0; i < first; i++) {
    for (u32 j = first; j < last; j++) {
      if (s->n_cps >= ATP_MAX_CPS) break;
      u32 nbuf = thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                           i, i + 1, j, j + 1,
                                           buf, ATP_CP_BATCH);
      pushed += atp_push_cps_traced(s, buf, nbuf,
                                    s->r_trace[i], s->r_trace[j]);
    }
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
