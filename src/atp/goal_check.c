// First conjunct whose goals_joined_mask bit is still clear.  Callers
// guarantee at least one unjoined goal (mask != all-set).
static u32 atp_goals_first_unjoined(const AtpState *s) {
  for (u32 g = 0; g < s->n_goals; g++) {
    if (!(s->goals_joined_mask & ((u64)1 << g))) return g;
  }
  return 0;
}

fn AtpStatus thvm_atp_goal_check(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0) return ATP_RUNNING;

  // Revision cache (see goal_check_running_rev in thvm.h): the goal's
  // joinability changes only when R changes.  The two goal_check calls per
  // step (top-of-step + post-cp-gen) at the same r_revision recompute the
  // same large-goal normalize; skip the redundant one.  Single universal
  // goal only; PROVED is never cached, so this can only delay, never miss,
  // a proof.
  if (!s->goal_existential && s->n_goals <= 1u
      && s->goal_check_running_rev == s->r_revision + 1u)
    return ATP_RUNNING;

  // 8.9c: existential goals use narrowing; universal goals stay
  // on the rewrite-and-compare path.
  if (s->goal_existential) {
    // Opt-in critical-goal queue (best-first narrowing, WM
    // PM_Existenzziele / GKC query_preference shape -- see the lane at
    // the end of this file).  The greedy walk below stays the default.
    if (s->use_narrow_queue) return atp_cgq_check(s);
    Term lhs = s->goal_lhs;
    Term rhs = s->goal_rhs;
    if (kbo_eq(lhs, rhs)) return ATP_PROVED;
    // Scope the narrowing proof recorder (see thvm.h nrec block) to
    // THIS loop: thvm_atp_narrow_step is public API on arbitrary
    // pairs, and only the goal chain belongs in the recording.
    s->narrow_rec_active = 1u;
    for (u32 i = 0; i < ATP_NARROW_BUDGET; i++) {
      Term new_lhs = 0, new_rhs = 0;
      u8 ok = thvm_atp_narrow_step(s, lhs, rhs,
                                   &new_lhs, &new_rhs,
                                   &s->witness_subst);
      if (!ok) {
        s->narrow_rec_active = 0u;
        return ATP_RUNNING;   // no more narrows; wait for new R
      }
      lhs = new_lhs;
      rhs = new_rhs;
      if (kbo_eq(lhs, rhs)) {
        // Capture the converged terms back into the goal slots
        // so successive goal_check calls (after full saturation)
        // still report PROVED.
        s->narrow_rec_active = 0u;
        s->goal_lhs = lhs;
        s->goal_rhs = rhs;
        return ATP_PROVED;
      }
    }
    // Budget exhausted; let the outer loop add more rules and retry.
    s->narrow_rec_active = 0u;
    s->goal_lhs = lhs;
    s->goal_rhs = rhs;
    return ATP_RUNNING;
  }

  // Multi-goal conjunction: check every conjunct not yet joined.  A
  // join is LATCHED in goals_joined_mask -- R only grows under
  // completion, so a once-joined goal stays joinable; re-checking it
  // would only burn normalization work.  PROVED only when every bit
  // is set.
  if (s->n_goals > 1u) {
    const u64 all_mask = (s->n_goals >= 64u)
        ? ~0ull : (((u64)1 << s->n_goals) - 1ull);
    for (u32 g = 0; g < s->n_goals; g++) {
      if (s->goals_joined_mask & ((u64)1 << g)) continue;
      if (atp_goal_pair_joined(s, s->goals_lhs[g], s->goals_rhs[g],
                               &s->goals_lhs_nf[g], &s->goals_rhs_nf[g])) {
        s->goals_joined_mask |= (u64)1 << g;
      }
#ifdef ATP_ORDERED_REWRITE
      else if (s->use_goal_narrow &&
               atp_goal_narrow_joined(s, s->goals_lhs[g],
                                      s->goals_rhs[g])) {
        s->goals_joined_mask |= (u64)1 << g;
      }
#endif
    }
    if (s->goals_joined_mask == all_mask) return ATP_PROVED;
    // Re-point the alias at the first unjoined conjunct so every
    // goal-directed heuristic (CPinGoal, goal-interleave, MNF) steers
    // toward a goal that still needs closing.
    atp_goals_alias(s, atp_goals_first_unjoined(s));
#ifdef ATP_MNF
    // The MNF front search runs against the CURRENT alias conjunct
    // only (its fronts are seeded from goal_lhs/goal_rhs);
    // atp_goals_alias above dropped a front set seeded from a
    // previously-aliased goal.  LOOP across conjuncts: each join
    // re-aliases to the next unjoined one (dropping the consumed
    // front set) and gives IT an MNF budget in the same call --
    // otherwise a conjunction whose every conjunct is MNF-only and
    // whose CP queue is already empty dies QUEUE_EMPTY after closing
    // just one conjunct per step.  Terminates: every successful
    // mnf_step latches one more mask bit; a failed one breaks.
    if (s->use_mnf) {
      for (;;) {
        if (s->mnf == NULL) s->mnf = mnf_create(s);
        if (s->mnf == NULL || !mnf_step(s, s->mnf, MNF_BUDGET)) break;
        s->goals_joined_mask |= (u64)1 << atp_goals_first_unjoined(s);
        if (s->goals_joined_mask == all_mask) return ATP_PROVED;
        atp_goals_alias(s, atp_goals_first_unjoined(s));
      }
    }
#endif
    return ATP_RUNNING;
  }

  if (atp_goal_pair_joined(s, s->goal_lhs, s->goal_rhs,
                           &s->goal_lhs_nf, &s->goal_rhs_nf)) {
    if (s->n_goals == 1u) {
      // Keep the (size-1) conjunct mirror coherent for consumers that
      // read per-goal state uniformly (wire reporting, proof side).
      s->goals_lhs_nf[0] = s->goal_lhs_nf;
      s->goals_rhs_nf[0] = s->goal_rhs_nf;
      s->goals_joined_mask = 1ull;
    }
    return ATP_PROVED;
  }
  // Refutational goal driver (opt-in): a backward condensed-detachment
  // regression over the raw ifeq-guarded equations, interleaved with
  // completion so the live R both bridges connective forms (via the
  // goal-side NF) and discharges the ifeq guards (against derived
  // theorem facts).  Closes goals the single-NF check structurally
  // cannot -- the closing chain needs modus-ponens regression, not
  // just rewriting.  Runs at most once per rule-set revision.
  if (s->use_goal_drive && atp_goal_drive_check(s) == ATP_PROVED) {
    if (s->n_goals == 1u) s->goals_joined_mask = 1ull;
    return ATP_PROVED;
  }
#ifdef ATP_ORDERED_REWRITE
  // Goal-narrow endgame (opt-in): a join here is one the reductive
  // single-NF check above structurally cannot see -- the closing
  // chain needs expanding ground steps.  Runs before the MNF front
  // (its incremental part is a few hash probes per check).
  if (s->use_goal_narrow &&
      atp_goal_narrow_joined(s, s->goal_lhs, s->goal_rhs)) {
    if (s->n_goals == 1u) s->goals_joined_mask = 1ull;
    return ATP_PROVED;
  }
#endif
#ifdef ATP_MNF
  // Milestone 10: the MNF bidirectional search AUGMENTS the single-NF
  // check -- it does not replace it.  goal_lhs seeds a GREEN front,
  // goal_rhs a RED one; each rewrites with R; an opposite-colour
  // collision is the join.  This is the only detector that can prove a
  // goal whose two sides never share one normal form -- a symmetric
  // goal like commutativity nand(x,y)=nand(y,x), where neither side
  // rewrites to the other, so the single-NF check above structurally
  // cannot fire.  (An earlier revision had MNF REPLACE the single-NF
  // check; that regressed thm, which MNF's front search did not close
  // but the single-NF check does.  Both are sound -- run both.)
  //
  // Gated on s->use_mnf so the dylib can be COMPILED with -DATP_MNF
  // (MNF linked in) yet stay completion-only by default: the front
  // search runs only when the WL surface flips the flag for
  // Method -> "GoalDirected".  Off, this block is a single branch test.
  if (s->use_mnf) {
    if (s->mnf == NULL) s->mnf = mnf_create(s);
    if (s->mnf != NULL && mnf_step(s, s->mnf, MNF_BUDGET)) {
      if (s->n_goals == 1u) s->goals_joined_mask = 1ull;
      return ATP_PROVED;
    }
  }
#endif
  // RUNNING at this R: cache the revision so the redundant top-of-step
  // call skips the re-normalize until R changes again.
  if (!s->goal_existential && s->n_goals <= 1u)
    s->goal_check_running_rev = s->r_revision + 1u;
  return ATP_RUNNING;
}

// 8.9c: set an existential conjecture.  Mirrors thvm_atp_set_goal
// but flips s->goal_existential so the narrow path runs in
// goal_check.
fn u8 thvm_atp_set_goal_existential(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  // A goal change invalidates the critical-goal queue (its CGs are
  // narrowed variants of the PREVIOUS conjecture).
  if (s->cgq != NULL) {
    atp_cgq_free(s->cgq);
    s->cgq = NULL;
  }
  if (lhs == 0) {
    s->goal_lhs = 0;
    s->goal_rhs = 0;
    s->goal_existential = 0;
    atp_goals_release(s);
#ifdef THVM_ATPFT_RULES
    s->goal_lhs_ft = NULL;
    s->goal_rhs_ft = NULL;
    s->goal_nf_ft_l = NULL;
    s->goal_nf_ft_r = NULL;
#endif
    return 1;
  }
  if (s->spec != NULL) {
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
  s->goal_lhs = lhs;
  s->goal_rhs = rhs;
  s->goal_existential = 1;
  // Pristine pair for the proof emitter (GC-rooted): the narrowing
  // lanes rewrite the goal slots destructively.  A goal change also
  // invalidates any recorded derivation.
  s->goal_orig_lhs = lhs;
  s->goal_orig_rhs = rhs;
  thvm_atp_nrec_free(s);
  s->tgt_ready = 0;   // tgt goal-symbol bitset recollected lazily
  // Existential goals are single-conjecture only: drop any multi-goal
  // state a prior thvm_atp_set_goals left behind so goal_check takes
  // the narrow path, not the conjunct loop.
  atp_goals_release(s);
#ifdef THVM_ATPFT_RULES
  // Stage 4: mirror the goal on the AtpFt side (same persistence /
  // arena story as thvm_atp_set_goal).
  s->goal_lhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, lhs, 0);
  s->goal_rhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, rhs, 0);
  s->goal_nf_ft_l = NULL;
  s->goal_nf_ft_r = NULL;
#endif
  return 1;
}

