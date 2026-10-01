// === 9.3: heap checkpoint/reset =====================================
fn u64 thvm_atp_heap_checkpoint(void) {
  return HEAP_NEXT;
}

fn void thvm_atp_heap_reset(u64 checkpoint) {
  // Only allow popping back; never advance (callers should use
  // term_new_* for that).  Silent no-op on out-of-range to make
  // the API safe to sprinkle in step paths.
  if (checkpoint <= HEAP_NEXT) {
    if (checkpoint < HEAP_NEXT) {
      // Popping the bump pointer recycles every heap loc in
      // [checkpoint, HEAP_NEXT): the next allocation reuses those loc
      // integers for new content.  The KBO weight memo (g_kbo_wmemo) is
      // keyed by (epoch, Term loc) and assumes a loc denotes one logical
      // term per epoch -- exactly the invariant a GC relocation breaks,
      // which is why thvm_atp_gc_collect invalidates the memo.  A reset
      // recycles locs the same way WITHOUT moving cells, so a memo entry
      // for a popped loc would survive and return the OLD term's weight
      // for the NEW term reallocated at that loc -- a wrong KBO verdict
      // that flips an order-gated rewrite (observed: the flatterm mixed
      // path's unorientable order-gate firing a non-decreasing step,
      // diverging from the tree NF).  Bump the epoch so every entry for a
      // recycled loc is stale and recomputed.
      thvm_kbo_invalidate();
    }
    HEAP_NEXT = checkpoint;
  }
}

// === 7a: in-loop GC for the saturation engine ======================
//
// thvm_atp_heap_checkpoint/reset only reclaims the per-step
// normalization scratch when a CP is trivially joined.  Every
// rule / CP / trace Term that survives a step is a heap-resident
// cell that lives forever, so a long completion run otherwise
// bumps HEAP_NEXT until heap_alloc reports "from-space exhausted".
//
// The fix: gather every live Term reachable from the AtpState into
// a root array and hand it to the Cheney collector (gc_collect).
// The collector evacuates each root to to-space, rewrites the root
// slot with the moved location, and swaps semi-spaces -- exactly
// the discipline `thvm_realize` uses for the WL session.  Writing
// the moved Terms back into the AtpState arrays keeps the engine
// pointing at the live copies.
//
// Live Term fields rooted here:
//   - lhs[0..n_rules), rhs[0..n_rules)        -- the rule set R
//   - goal_lhs, goal_rhs                      -- the conjecture
//   - goals_lhs/rhs[/..._nf][0..n_goals)      -- multi-goal conjuncts
//   - trace[0..n_trace)                       -- TAG_CTR entries
//                                                (each holds lhs/rhs)
//   - witness_subst.bindings[0..REWRITE_MAX_VAR) -- narrowing σ
//
// The CP queue is NOT rooted: after the Waldmeister Stringterms port
// each queued CP is a packed byte string in plain malloc memory
// (cp_packed[]), outside the managed heap, so the collector never
// touches it.  This is the structural fix for the late-game GC wall.
// The subsumption-index records borrow those byte strings (u8 *, not
// Term), so the index needs no rooting either.
//
// Returns 1 if a collection ran, 0 if GC is disabled / no state.
fn u8 thvm_atp_gc_collect(AtpState *s) {
  if (s == NULL || !gc_enabled()) return 0;
  u64 gc_t0 = atp_now_us();
  // Trace entries past the default cap live in the permanent heap tail
  // (atp_trace_node) and need no collection: rooting only the default-
  // cap prefix keeps a proof run's collection work the no-proof run's.
  u32 n_trace_roots = (s->trace_x_heap || s->n_trace < ATP_MAX_TRACE)
                        ? s->n_trace : ATP_MAX_TRACE;

  // needle (THVM_ATP_DRC_NEEDLE): fold_def_body[0..fold_width) and
  // needle_ax_l/r[0..n_needle_ax) are long-lived Term arrays (set once
  // at problem intake, read by every DRC keep decision thereafter), so
  // every GC must relocate them exactly like the rule set.  fold_width
  // is clamped defensively even though thvm_atp_set_fold_band /
  // thvm_atp_set_fold_defs never exceed ATP_NEEDLE_MAX_DEFS together.
  u32 needle_fold_width = (s->fold_hi > s->fold_lo)
                           ? (s->fold_hi - s->fold_lo) : 0u;
  if (needle_fold_width > ATP_NEEDLE_MAX_DEFS) needle_fold_width = ATP_NEEDLE_MAX_DEFS;
  // Count the root slots so we can size the array exactly.
  // 2 per rule for lhs/rhs, plus 2 per rule for dead-save (originals
  // are kept live so proof reconstruction can read them when the slot
  // has been soft-deleted by backward subsumption).  +1 for
  // s->min_const (the FVI reserved constant; allocated once in
  // thvm_atp_init and reused across orient_and_add calls, so a GC
  // mid-completion would otherwise leave a dangling cell).
  u32 n_roots = 6u * s->n_rules /* lhs,rhs,dead l/r save,subsumer l/r */
              + 6u /* goal + goal_nf + pristine goal_orig pair */
              + 4u * s->n_goals /* multi-goal conjuncts + NFs */
              + n_trace_roots + REWRITE_MAX_VAR
              + 1u /* min_const */
              + 2u * s->n_irv /* buffered IR victims (use_wm_demote) */
              + needle_fold_width + 2u * s->n_needle_ax /* needle */
              + (s->gc_extra_roots != NULL ? s->n_gc_extra_roots : 0u);
#ifdef ATP_MNF
  // Milestone 10: every MNF coloured node holds a reached Term.  Root
  // them so the collector relocates them; the hash table (structural
  // hashes, node indices) is GC-invariant and needs no fixup.
  n_roots += mnf_gc_count(s->mnf);
#endif
#ifdef ATP_ORDERED_REWRITE
  // Goal-narrow E-graph nodes each hold a representative Term.  The
  // congruence machinery (labels, kid ids, union-find, signature
  // hashes over class ids) is GC-invariant and needs no fixup.
  n_roots += gn_gc_count(s->gnarrow);
#endif
  // Critical-goal queue: two Terms per stored CG.
  n_roots += cgq_gc_count(s->cgq);
  // Top-K re-ranker: the goal normal forms it scores CPs against.
  n_roots += atp_topk_gc_count(s);
  Term *roots = (Term *)malloc((size_t)n_roots * sizeof(Term));
  if (roots == NULL) return 0;

  u32 w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    roots[w++] = s->lhs[i];
    roots[w++] = s->rhs[i];
  }
  // Dead-rule originals are appended after the live-rule block so the
  // collector relocates them too.  0 (unused slot for never-dead rules)
  // is treated as a null root by gc_collect.
  for (u32 i = 0; i < s->n_rules; i++) {
    roots[w++] = s->r_dead_lhs_save[i];
    roots[w++] = s->r_dead_rhs_save[i];
    roots[w++] = s->r_dead_subsumer_lhs[i];
    roots[w++] = s->r_dead_subsumer_rhs[i];
  }
  roots[w++] = s->goal_lhs;
  roots[w++] = s->goal_rhs;
  roots[w++] = s->goal_lhs_nf;
  roots[w++] = s->goal_rhs_nf;
  roots[w++] = s->goal_orig_lhs;
  roots[w++] = s->goal_orig_rhs;
  // Multi-goal conjuncts.  goals_lhs[0] usually VALUE-aliases goal_lhs
  // (same cell); the collector's forwarding pointers relocate both
  // copies to the same to-space address, so duplicate roots are safe
  // and cost no extra copying.  0 entries (unset NFs) are null roots.
  for (u32 g = 0; g < s->n_goals; g++) {
    roots[w++] = s->goals_lhs[g];
    roots[w++] = s->goals_rhs[g];
    roots[w++] = s->goals_lhs_nf[g];
    roots[w++] = s->goals_rhs_nf[g];
  }
  for (u32 i = 0; i < n_trace_roots; i++) roots[w++] = s->trace[i];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    roots[w++] = s->witness_subst.bindings[i];
  }
  // FVI reserved constant: cached in init, used in orient_and_add.
  // Must be relocated by every GC so the cached Term stays valid.
  u32 min_const_root = w;
  roots[w++] = s->min_const;
  // Buffered IR victims (use_wm_demote): live only between interreduce
  // and the post-CP-gen drain, but the heap-pressure GC can run inside
  // generate_cps in exactly that window.
  for (u32 i = 0; i < s->n_irv; i++) {
    roots[w++] = s->irv_lhs[i];
    roots[w++] = s->irv_rhs[i];
  }
  // needle (THVM_ATP_DRC_NEEDLE): fold-band definition bodies + the
  // pre-tgt axiom list (see needle_fold_width above).  0 entries when
  // needle was never armed for this problem -- null roots, same as
  // every other unset-slot loop here.
  u32 needle_root = w;
  for (u32 i = 0; i < needle_fold_width; i++) roots[w++] = s->fold_def_body[i];
  for (u32 i = 0; i < s->n_needle_ax; i++) {
    roots[w++] = s->needle_ax_l[i];
    roots[w++] = s->needle_ax_r[i];
  }
  // Caller-owned extra roots (see the thvm.h field comment): evacuated
  // like every other root and written back in place below, so the
  // caller's handles stay live across the collection.  NULL/0 keeps the
  // historical root set byte-identical.
  u32 extra_root_base = w;
  if (s->gc_extra_roots != NULL) {
    for (u32 i = 0; i < s->n_gc_extra_roots; i++) {
      roots[w++] = s->gc_extra_roots[i];
    }
  }
#ifdef ATP_MNF
  u32 mnf_node_root = w;
  mnf_gc_gather(s->mnf, roots, &w);
#endif
#ifdef ATP_ORDERED_REWRITE
  u32 gn_node_root = w;
  gn_gc_gather(s->gnarrow, roots, &w);
#endif
  u32 cgq_node_root = w;
  cgq_gc_gather(s->cgq, roots, &w);
  u32 topk_root = w;
  atp_topk_gc_gather(s, roots, &w);

  gc_collect(roots, w);
  g_atp_gc_epoch++;   // invalidate handle-keyed caches (cells moved)

  // Per-collect swap probe (THVM_ATP_RECYCLE_DEBUG): each gc_collect
  // swaps the Cheney semi-spaces, so after this call from_start
  // toggles between 0 (lower) and space_words (upper).  Confirms the
  // odd-swap parity a following ATP run inherits.
  { static int sw = -1;
    if (sw < 0) sw = (getenv("THVM_ATP_RECYCLE_DEBUG") != NULL) ? 1 : 0;
    if (sw)
      fprintf(stderr, "[atp-gccollect] gc_count=%llu from_start=%llu "
                      "HEAP_NEXT=%llu s=%p step=%u n_rules=%u n_cps=%u\n",
        (unsigned long long)gc_count(),
        (unsigned long long)gc_from_start(),
        (unsigned long long)HEAP_NEXT, (void *)s, s->step, s->n_rules,
        s->n_cps); fflush(stderr); }

  // DIAG: post-GC live-set breakdown (NORMLOG).  Sum the trace-term
  // node counts vs the rule-term node counts to see which dominates.
  { static int dl = -1;
    if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
    if (dl) {
      u64 rule_n = 0, trace_n = 0;
      for (u32 i = 0; i < s->n_rules; i++)
        rule_n += atp_symbol_count(roots[2u*i]) + atp_symbol_count(roots[2u*i+1u]);
      u32 tbase = 6u*s->n_rules + 4u + 4u*s->n_goals;
      for (u32 i = 0; i < n_trace_roots; i++) trace_n += atp_symbol_count(roots[tbase + i]);
      fprintf(stderr, "GCLOG n_rules=%u n_trace=%u live=%lluK rule_nodes=%lluK trace_nodes=%lluK\n",
        s->n_rules, s->n_trace, (unsigned long long)((HEAP_NEXT - gc_from_start())>>10),
        (unsigned long long)(rule_n>>10), (unsigned long long)(trace_n>>10)); } }
  // Write the relocated Terms back into the AtpState in the same
  // order they were gathered.
  w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    s->lhs[i] = roots[w++];
    s->rhs[i] = roots[w++];
  }
  for (u32 i = 0; i < s->n_rules; i++) {
    s->r_dead_lhs_save[i] = roots[w++];
    s->r_dead_rhs_save[i] = roots[w++];
    s->r_dead_subsumer_lhs[i] = roots[w++];
    s->r_dead_subsumer_rhs[i] = roots[w++];
  }
  s->goal_lhs = roots[w++];
  s->goal_rhs = roots[w++];
  s->goal_lhs_nf = roots[w++];
  s->goal_rhs_nf = roots[w++];
  s->goal_orig_lhs = roots[w++];
  s->goal_orig_rhs = roots[w++];
  for (u32 g = 0; g < s->n_goals; g++) {
    s->goals_lhs[g]    = roots[w++];
    s->goals_rhs[g]    = roots[w++];
    s->goals_lhs_nf[g] = roots[w++];
    s->goals_rhs_nf[g] = roots[w++];
  }
  for (u32 i = 0; i < n_trace_roots; i++) s->trace[i] = roots[w++];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    s->witness_subst.bindings[i] = roots[w++];
  }
  s->min_const = roots[w++];
  (void)min_const_root;
  for (u32 i = 0; i < s->n_irv; i++) {
    s->irv_lhs[i] = roots[w++];
    s->irv_rhs[i] = roots[w++];
  }
  (void)needle_root;
  for (u32 i = 0; i < needle_fold_width; i++) s->fold_def_body[i] = roots[w++];
  for (u32 i = 0; i < s->n_needle_ax; i++) {
    s->needle_ax_l[i] = roots[w++];
    s->needle_ax_r[i] = roots[w++];
  }
  if (s->gc_extra_roots != NULL) {
    for (u32 i = 0; i < s->n_gc_extra_roots; i++) {
      s->gc_extra_roots[i] = roots[extra_root_base + i];
    }
  }
  (void)extra_root_base;
#ifdef ATP_MNF
  // Write the relocated reached-Terms back into the MNF nodes.  No
  // hash-table fixup: mnf_hash is structural, so a relocated term
  // keeps its hash and stays in its bucket.
  mnf_gc_writeback(s->mnf, roots, mnf_node_root);
#endif
#ifdef ATP_ORDERED_REWRITE
  gn_gc_writeback(s->gnarrow, roots, gn_node_root);
#endif
  cgq_gc_writeback(s->cgq, roots, cgq_node_root);
  atp_topk_gc_writeback(s, roots, topk_root);

  free(roots);

  // Cells moved: the persistent LPO (s,t)->verdict memo and the per-term
  // KBO weight memo are both keyed on cell addresses, now stale -- drop them.
  thvm_lpo_invalidate();
  thvm_kbo_invalidate();
  // Stage 8 orient cache is keyed on atp_term_struct_hash (GC-stable by
  // construction), so the entries would still be sound -- but the
  // workload changes at this boundary (rules churned, GC mid-saturation)
  // and the cache's contents are no longer representative of the next
  // phase.  Bump the epoch; cheap (one counter inc).
  ATP_ORIENT_CACHE_INVAL();
  // The normalize-result cache stores Term values (cell addresses), so
  // GC relocation makes its entries stale.  Bump the unf_memo_epoch so
  // every cached NF is invalidated.  Whole-subject + per-position
  // no-fire memos are hash-keyed (cell-relocation-safe), but we still
  // force-bump the step epoch here: GC is a coarse phase boundary
  // (workload shifts post-GC) and the rare-event cost is negligible.
  atp_unf_memo_invalidate(s->r_revision, 1u);
  s->gc_live_after = HEAP_NEXT - gc_from_start();
  g_atp_gc_us += atp_now_us() - gc_t0;
  g_atp_gc_n  += 1u;
  return 1;
}

// Live-set fraction of the semi-space past which a collection that
// reclaimed too little ends the run (THVM_ATP_HEAP_ABORT_FRAC, default
// 0.85, valid (0.5, 1.0); 0 disables -- the sentinel 2.0 never trips).
static double atp_heap_abort_frac(void) {
  static double frac = -1.0;
  if (frac < 0.0) {
    frac = 0.85;
    const char *e = getenv("THVM_ATP_HEAP_ABORT_FRAC");
    if (e != NULL && *e != '\0') {
      char *end = NULL;
      double v = strtod(e, &end);
      if (end != NULL && *end == '\0' && v > 0.5 && v < 1.0) {
        frac = v;
      } else if (end != NULL && *end == '\0' && v == 0.0) {
        frac = 2.0;
      }
    }
  }
  return frac;
}

// Trigger threshold: collect when the from-space bump cursor has
// crossed this fraction of the live semi-space.  Half-full mirrors
// the default trigger in `thvm_realize` (realize.c).
static u8 atp_heap_under_pressure(void) {
  if (!gc_enabled()) return 0;
  u64 lo   = gc_from_start();
  u64 hi   = gc_from_end();
  u64 half = lo + (hi - lo) / 2;
  return HEAP_NEXT > half;
}

// In-step collection under heap pressure (the overlap-pair loops, the
// Otter replay).  A live set that a collection leaves above half the
// semi-space (the on-heap trace under --proof) would otherwise collect
// again on every pair: once the last collection left it above half,
// the next one waits for another eighth of the space (or the last
// thirty-second).  A collection that leaves the live set above
// atp_heap_abort_frac() raises heap_full; the step ends the run
// (ResourceOut) at its next check instead of thrashing.
static void atp_pressure_collect(AtpState *s) {
  if (!atp_heap_under_pressure()) return;
  u64 lo = gc_from_start(), cap = gc_from_end() - lo, used = HEAP_NEXT - lo;
  if (s->gc_live_after > cap / 2u && used < s->gc_live_after + cap / 8u &&
      used < cap - cap / 32u) {
    return;
  }
  thvm_atp_gc_collect(s);
  lo  = gc_from_start();
  cap = gc_from_end() - lo;
  if ((double)(HEAP_NEXT - lo) > atp_heap_abort_frac() * (double)cap) {
    s->heap_full = 1u;
  }
}

// Grow the heap-allocated trace[] to hold at least one more entry,
// honoring the s->t_max soft cap.  Returns 1 if there is room for the
// next push, 0 if the cap is hit (caller returns ATP_TRACE_NONE).
static int atp_trace_ensure(AtpState *s) {
  if (s->n_trace >= s->t_max) return 0;
  if (s->n_trace < s->t_cap) return 1;
  u32 cap = s->t_cap ? s->t_cap * 2u : 4096u;
  if (cap > s->t_max) cap = s->t_max;
  Term *nt = (Term *)realloc(s->trace, (size_t)cap * sizeof(Term));
  if (nt == NULL) return 0;
  s->trace = nt;
  if (s->trace_pack) {
    u64 *no = (u64 *)realloc(s->trace_eq_off, (size_t)cap * sizeof(u64));
    if (no == NULL) return 0;
    // Zero the freshly grown tail (0 = "no packed pair").
    for (u32 k = s->t_cap; k < cap; k++) no[k] = 0u;
    s->trace_eq_off = no;
  }
  s->t_cap = cap;
  return 1;
}

// Off-heap trace-equation store (THVM_ATP_TRACE_PACK).  Append the packed
// (lhs,rhs) pair to s->trace_eq_buf and return its byte offset + 1 (so 0
// stays the "none" sentinel).  acp_pack_term is self-delimiting via arity,
// so the loader reads lhs then rhs back-to-back.
static u64 atp_trace_eq_store(AtpState *s, Term lhs, Term rhs) {
  // Pack into the shared scratch first (acp_pack_term emits there),
  // then append the exact bytes -- no 21-bytes/node bound pre-walk.
  u32 pos = 0u, nodes = 0u;
  u8 stored = 0u;
  if (g_trace_eq_nv) {
    if (g_vcp_emit.armed) {
      // FT-direct push: the armed CP's packed pair
      // already sits in the scratch (vcp_emit_fused) -- the exact
      // bytes the renumber-during-emit walk below would produce; the
      // raw>=50 class stores its raw pair == its queued pair.
      pos = g_vcp_emit.len;
      stored = 1u;
    } else if (g_vcp_treat.armed) {
      // Treated FT-direct push: emit the renumbered RAW pair bytes NOW
      // (deferred from formation -- only survivors reach this store;
      // the joined third never pays the emit).  Byte-identical to the
      // formation-time emit: same operands, same still-armed subst.
      // A stash with raw_len != 0 already carries its raw bytes at
      // g_acp_scratch[0, raw_len) -- the Term-arm treat form's eager
      // emit and the ifeq-strip emit hijack (no formation operands to
      // defer to) -- so consume them instead of re-emitting from the
      // then-unset lazy-emit operands.
      pos = g_vcp_treat.raw_len;
      if (pos == 0u) {
        pos = vcp_ft_treat_emit_raw(s);
        if (pos == 0u) return 0u;   // emit anomaly: no-pair sentinel
        g_vcp_treat.raw_len = pos;
      }
      stored = 1u;
    } else {
      // Fused push loop: the raw pair arrives un-renumbered; renumber
      // during the byte emit (bytes identical to normalize-then-pack).
      stored = acp_trace_pack_fused(lhs, rhs, &pos);
      if (!stored) {
        pos = 0u;
        thvm_normalize_vars(&lhs, &rhs);   // anomaly: classic pipeline
      }
    }
  }
  if (!stored) {
    acp_pack_term(lhs, &pos, &nodes);
    acp_pack_term(rhs, &pos, &nodes);
  }
  if (s->trace_eq_len + pos > s->trace_eq_cap) {
    u64 ncap = s->trace_eq_cap ? s->trace_eq_cap * 2u : (1u << 20);
    while (s->trace_eq_len + pos > ncap) ncap *= 2u;
    u8 *nb = (u8 *)realloc(s->trace_eq_buf, (size_t)ncap);
    if (nb == NULL) thvm_fatal("atp_trace_eq_store: OOM");
    s->trace_eq_buf = nb;
    s->trace_eq_cap = ncap;
  }
  u64 off = s->trace_eq_len;
  memcpy(s->trace_eq_buf + off, g_acp_scratch, pos);
  s->trace_eq_len = off + pos;
  return off + 1u;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span-sourced sibling of atp_trace_eq_store: pack the pair's acp bytes
// straight from spans (vcp_ft_emit_side with a fresh shared map -- on
// var-normed spans the appearance-order renumber is the identity, so
// the bytes equal acp_pack_term of the canonical pair; on raw spans it
// equals the renumber-during-emit walk atp_trace_eq_store itself uses
// for the fused push).  0 on an emit anomaly -- caller falls back to a
// Term-sourced store.
static u64 atp_trace_eq_store_ft(AtpState *s, const AtpFtCell *lhs,
                                 const AtpFtCell *rhs) {
  AcpNvMap m;
  m.n = 0u;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  if (!vcp_ft_emit_side(lhs, NULL, NULL, NULL, s->kbo, &m,
                        &pos, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (!vcp_ft_emit_side(rhs, NULL, NULL, NULL, s->kbo, &m,
                        &pos, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (s->trace_eq_len + pos > s->trace_eq_cap) {
    u64 ncap = s->trace_eq_cap ? s->trace_eq_cap * 2u : (1u << 20);
    while (s->trace_eq_len + pos > ncap) ncap *= 2u;
    u8 *nb = (u8 *)realloc(s->trace_eq_buf, (size_t)ncap);
    if (nb == NULL) thvm_fatal("atp_trace_eq_store_ft: OOM");
    s->trace_eq_buf = nb;
    s->trace_eq_cap = ncap;
  }
  u64 off = s->trace_eq_len;
  memcpy(s->trace_eq_buf + off, g_acp_scratch, pos);
  s->trace_eq_len = off + pos;
  return off + 1u;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// Reconstruct entry `idx`'s (lhs,rhs) from the off-heap store as live IC
// Terms.  Returns 0 if the entry has no packed pair (sentinel 0).
static int atp_trace_eq_load(AtpState *s, u32 idx, Term *lhs, Term *rhs) {
  if (!s->trace_pack || s->trace_eq_off == NULL) return 0;
  u64 o = s->trace_eq_off[idx];
  if (o == 0u) return 0;
  const u8 *p = s->trace_eq_buf + (o - 1u);
  *lhs = acp_unpack_term(&p);
  *rhs = acp_unpack_term(&p);
  return 1;
}

// Materialize the off-heap packed trace: rebuild packed entries with
// their real (lhs,rhs) loaded from the byte store, replacing the
// NUM sentinels in children[2..3].  The trace is kept PACKED during
// saturation (small live set -> no proof-trace GC-heap blowup, the
// [[project_atp_wm_trace_memory_blocker]] bottleneck) and materialized
// exactly ONCE here, after thvm_atp_run terminates, so the C proof
// extractor + the WL trace reader see live Term entries.
//
// CONE-SCOPED (read-set audited against ATP_ProofGraph.wl): the WL
// lift decodes a trace entry's (lhs, rhs) Term handles in exactly two
// ways --
//   (a) population-wide over TRACE_ORIENT / TRACE_FVI entries only
//       (aliveRulesAt + the two allRules emitNorm retries build rule
//       tables from every ORIENT/FVI ever born), and
//   (b) the resolveTrace recursion, whose roots are live-rule birth
//       entries (resolveRule -> rTrace[k]) and which otherwise only
//       moves to an already-visited entry's parents (children 0/1;
//       a NORM_STEP's child 1 is a rule's TRACE_ORIENT index).
// Reason / parents / pos ship as wire ints and never touch the term
// handles, so every OTHER entry -- the ~millions of queued-CP
// TRACE_CP records plus uncited SIMPLIFY entries -- never has its
// pair decoded, and rebuilding it into live Terms is pure waste
// (the 0.48s materialize-trace tile on OrAssociativity: 1.98M
// entries vs a ~thousands-entry read set).  Mark the read set (all
// ORIENT/FVI + all rule births + the parent closure) and materialize
// only it; unmarked entries keep their NUM sentinels, which the wire
// encoder, GC root walk, and orphan scan already handle (they touch
// only tags/handles, not the pair content).  An MNF-closed goal cites
// TRACE indices directly from the goal chain (usingMnf), so use_mnf
// keeps the historical full materialization.
// Kill switch: THVM_ATP_NO_CONE_MAT=1 -> full materialization.
//
// GC-safe: runs OUTSIDE the step loop, so no mid-loop collection moves
// the children snapshot.  One compaction up front frees dead CPs so the
// rebuilt trace fits in the from-space alongside the live set; the loop
// then only bumps (the live set + materialized trace stays well under
// the semispace, and mid-op heap_alloc never auto-collects).
fn void thvm_atp_materialize_trace(AtpState *s) {
  if (s == NULL || !s->trace_pack) return;
  if (gc_enabled()) thvm_atp_gc_collect(s);
  static int no_cone = -1;
  if (no_cone < 0) {
    const char *ev = getenv("THVM_ATP_NO_CONE_MAT");
    no_cone = (ev != NULL && ev[0] == '1') ? 1 : 0;
  }
  u8 *mark = NULL;
  if (!no_cone && !s->use_mnf && s->n_trace > 0u) {
    mark = (u8 *)calloc(s->n_trace, 1u);
    u32 *stk = (mark != NULL)
      ? (u32 *)malloc((size_t)s->n_trace * sizeof(u32)) : NULL;
    if (stk == NULL) {
      free(mark);
      mark = NULL;                      // OOM: degrade to full materialize
    } else {
      u32 sp = 0u;
      // Seed (a): every ORIENT / FVI entry (population-wide WL reads).
      for (u32 i = 0; i < s->n_trace; i++) {
        Term e = s->trace[i];
        if (term_tag(e) != TAG_CTR) continue;
        u32 reason = term_ext(e);
        if (reason != TRACE_ORIENT && reason != TRACE_FVI) continue;
        mark[i] = 1u;
        stk[sp++] = i;
      }
      // Seed (b): live-rule birth entries (resolveRule roots).
      for (u32 r = 0; r < s->n_rules; r++) {
        u32 ti = s->r_trace[r];
        if (ti != ATP_TRACE_NONE && ti < s->n_trace && !mark[ti]) {
          mark[ti] = 1u;
          stk[sp++] = ti;
        }
      }
      // Parent closure: children 0/1 of every marked entry are trace
      // indices for the 4-child reasons; a NORM_STEP's child 1 is a
      // rule's TRACE_ORIENT trace index (already seeded, followed
      // anyway -- over-marking is harmless).
      while (sp > 0u) {
        u32 ti = stk[--sp];
        Term e = s->trace[ti];
        if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) continue;
        for (u32 k = 0; k < 2u; k++) {
          u32 p = (u32)term_val(term_ctr_at(e, k));
          if (p != ATP_TRACE_NONE && p < s->n_trace && !mark[p]) {
            mark[p] = 1u;
            stk[sp++] = p;
          }
        }
      }
      free(stk);
    }
  }
  u32 n_mat = 0u;
  for (u32 i = 0; i < s->n_trace; i++) {
    if (mark != NULL && !mark[i]) continue;
    Term lhs, rhs;
    if (!atp_trace_eq_load(s, i, &lhs, &rhs)) continue;   // no packed pair
    Term e = s->trace[i];
    u32 reason = term_ext(e);
    u32 n = term_ctr_n(e);
    if (n < 4u || n > 63u) continue;                      // defensive
    Term ch[64];
    for (u32 k = 0; k < n; k++) ch[k] = term_ctr_at(e, k);
    ch[2] = lhs;
    ch[3] = rhs;
    s->trace[i] = term_new_ctr(reason, ch, n);
    if (i >= ATP_MAX_TRACE) s->trace_x_heap = 1u;
    s->trace_eq_off[i] = 0u;                              // now materialized
    n_mat++;
  }
  if (getenv("THVM_ATP_TIME_SPLIT") != NULL ||
      getenv("THVM_ATP_PHASETIME") != NULL) {
    fprintf(stderr, "[atp-phase] cone-mat: materialized=%u of %u (cone=%s)\n",
            n_mat, s->n_trace, mark != NULL ? "on" : "off");
  }
  free(mark);
  s->trace_pack = 0u;
}

// Trace entries past the default cap (ATP_MAX_TRACE) exist only when a
// proof raised it: the no-proof search stops recording there.  Such an
// entry keeps the search out of its way -- its node goes to the
// permanent heap tail (never copied by the collector), its work runs
// inside a work-clock exclusion window, and its bytes are discounted by
// the RSS guard -- so the search sees the default-cap heap and clock.
// The window opens at every push attempt past the default cap, in the
// no-proof run too (where the attempt just fails): a window's own
// entry/exit work then counts the same in both runs.
static u8 atp_trace_windowed(const AtpState *s) {
  return (u8)(s->n_trace >= ATP_MAX_TRACE);
}
static u8 atp_trace_proof_only(const AtpState *s) {
  return (u8)(s->n_trace >= ATP_MAX_TRACE && s->t_max > ATP_MAX_TRACE);
}

// Entry node: atom-only nodes of a proof-only entry take the permanent
// tail (the collector never scans it); anything else, or a full tail,
// takes the ordinary heap.
static Term atp_trace_node(AtpState *s, u8 proof_only, u32 reason,
                           const Term *ch, u32 n) {
  if (proof_only) {
    u32 i = 0;
    while (i < n && term_tag(ch[i]) == TAG_NUM) i++;
    if (i == n) {
      u64 loc = heap_perm_alloc(1u + n);
      if (loc != 0u) {
        heap_set(loc, term_new(0, TAG_NUM, DT_INT32, n));
        for (u32 k = 0; k < n; k++) heap_set(loc + 1u + k, ch[k]);
        s->trace_x_bytes += (u64)(1u + n) * sizeof(Term);
        return term_new(0, TAG_CTR, reason, loc);
      }
    }
    s->trace_x_heap = 1u;
  }
  return term_new_ctr(reason, ch, n);
}

// Push a trace entry as a TAG_CTR with label = reason and children
// [NUM(parent_a), NUM(parent_b), lhs, rhs].  Returns the entry's
// index in s->trace, or ATP_TRACE_NONE if the buffer is full.
//
// 6.1b/c will wire this into add_equation / orient_and_add /
// generate_cps; for 6.1a the helper just exists, and the storage is
// init'd to zero by thvm_atp_init's calloc.
static u32 atp_trace_push_impl(AtpState *s, u8 proof_only, u32 reason,
                               u32 p_a, u32 p_b, Term lhs, Term rhs);
static u32 atp_trace_push(AtpState *s, u32 reason, u32 p_a, u32 p_b,
                          Term lhs, Term rhs) {
  if (s == NULL) return ATP_TRACE_NONE;
  u8 windowed = atp_trace_windowed(s);
  if (windowed) atp_work_pause();
  u32 idx = atp_trace_push_impl(s, atp_trace_proof_only(s), reason, p_a,
                                p_b, lhs, rhs);
  if (windowed) atp_work_resume();
  return idx;
}
static u32 atp_trace_push_impl(AtpState *s, u8 proof_only, u32 reason,
                               u32 p_a, u32 p_b, Term lhs, Term rhs) {
  if (!atp_trace_ensure(s)) return ATP_TRACE_NONE;
  u64 eq0 = s->trace_eq_len;
  // Rule/axiom/ORIENT/SIMPLIFY entries stay MATERIALIZED (real pair on
  // heap) even under trace_pack.  These are only ~n_rules entries (the
  // millions that drive the GC live set are the CP pushes, packed in
  // atp_trace_push_cp), so keeping them real costs no meaningful RSS --
  // and it MUST be real: off-heap-packing the axioms plants NUM
  // sentinels for the pre-first-CP trace, which shifts the heap layout
  // the CP former reads and perturbs CP FORMATION ORDER from seq=0 (the
  // whole BooleanAxioms theory diverged from wmcli at the first pick
  // once TracePack became the default).  This mirrors the cp_trace_lean
  // split (rules materialized, CPs leaned), which is selection-faithful.
  u64 eq_off = 0u;
  Term c_lhs = lhs, c_rhs = rhs;
  // THVM_ATP_TRACE_LEAN_RULES=1 (experiment): pack the rule-birth pair
  // off-heap like the CP entries, sentinels in the children.  The
  // historical reason these stay materialized -- the TERM CP former
  // read the heap layout, and leaning the axioms perturbed formation
  // order from seq=0 -- may have dissolved now that formation runs on
  // spans (the FT arm); the parity suite under this env is the test.
  static int lean_rules = -1;
  if (lean_rules < 0) {
    // Default ON (see atp_lazy_rules_on); "0" is the kill switch.
    const char *ln = getenv("THVM_ATP_TRACE_LEAN_RULES");
    lean_rules = (ln == NULL || ln[0] != '0');
  }
  if (s->trace_pack && lean_rules) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (g_trace_eq_ft_l != NULL && g_trace_eq_ft_r != NULL) {
      eq_off = atp_trace_eq_store_ft(s, g_trace_eq_ft_l, g_trace_eq_ft_r);
    }
    if (eq_off == 0u)
#endif
    {
      eq_off = atp_trace_eq_store(s, lhs, rhs);
    }
    c_lhs = term_new(0, TAG_NUM, 0, 0);   // sentinel; real pair off-heap
    c_rhs = c_lhs;
  }
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  g_trace_eq_ft_l = NULL;
  g_trace_eq_ft_r = NULL;
#endif
  Term children[4] = {
    term_new(0, TAG_NUM, 0, p_a),
    term_new(0, TAG_NUM, 0, p_b),
    c_lhs,
    c_rhs,
  };
  s->trace[s->n_trace] = atp_trace_node(s, proof_only, reason, children, 4);
  u32 idx = s->n_trace;
  if (s->trace_pack) s->trace_eq_off[idx] = eq_off;   // 0 = real pair on heap
  s->n_trace++;
  if (proof_only)
    s->trace_x_bytes += (s->trace_eq_len - eq0) + 2u * sizeof(u64);
  if (reason == TRACE_ORIENT || reason == TRACE_FVI || reason == TRACE_SIMPLIFY)
    atp_rj_view(s, idx);
  atp_hx_trace_stamp(s, idx);
  return idx;
}

// Push a TRACE_CP entry that also carries the superposition position
// -- pos[0..pos_len) is the path into parent_a's rule lhs where
// parent_b's rule lhs overlapped (CriticalPair.pos).  The entry is a
// TAG_CTR(TRACE_CP) with children
//   [NUM(p_a), NUM(p_b), lhs, rhs, NUM(pos_len), NUM(pos_0), ...,
//    NUM(combo)].
// Children 0..3 match a plain atp_trace_push entry, so the trace
// serializer, GC root walk, and orphan-kill scan -- all of which
// touch only the first four children -- are unaffected; the proof
// DAG reads the overlap geometry off children 4..4+pos_len.  The final
// child (index 5+pos_len) is the overlap face-combo index (0..3, or
// 0xff = unknown) -- the only consumer is atp_wmo_cp_combo, which the
// CP-formation side geometry (use_cp_side) reads at the orient site.
static u32 atp_trace_push_cp_impl(AtpState *s, u8 proof_only,
                                  u32 p_a, u32 p_b, Term lhs, Term rhs,
                                  const u8 *pos, u8 pos_len, u8 combo);
static u32 atp_trace_push_cp(AtpState *s, u32 p_a, u32 p_b,
                             Term lhs, Term rhs,
                             const u8 *pos, u8 pos_len, u8 combo) {
  if (s == NULL) return ATP_TRACE_NONE;
  u8 windowed = atp_trace_windowed(s);
  if (windowed) atp_work_pause();
  u32 idx = atp_trace_push_cp_impl(s, atp_trace_proof_only(s), p_a, p_b,
                                   lhs, rhs, pos, pos_len, combo);
  if (windowed) atp_work_resume();
  return idx;
}
static u32 atp_trace_push_cp_impl(AtpState *s, u8 proof_only,
                                  u32 p_a, u32 p_b, Term lhs, Term rhs,
                                  const u8 *pos, u8 pos_len, u8 combo) {
  if (!atp_trace_ensure(s)) return ATP_TRACE_NONE;
  u64 eq0 = s->trace_eq_len;
  u64 eq_off = 0u;
  Term c_lhs = lhs, c_rhs = rhs;
  if (s->trace_pack) {
    eq_off = atp_trace_eq_store(s, lhs, rhs);
    c_lhs = term_new(0, TAG_NUM, 0, 0);   // sentinel; real pair off-heap
    c_rhs = c_lhs;
  } else if (s->cp_trace_lean) {
    // Lean CP trace: plant sentinels and store the pair NOWHERE.  The
    // queued CP keeps its terms in cp_packed (selection unpacks them
    // there); the GC no longer roots the millions of CP-term pairs that
    // dominate the live set on a deep completion.  Only the CP push is
    // leaned -- rule (ORIENT/AXIOM) trace terms stay materialized.
    c_lhs = term_new(0, TAG_NUM, 0, 0);
    c_rhs = c_lhs;
  }
  Term children[6 + CP_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, p_b);
  children[2] = c_lhs;
  children[3] = c_rhs;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5 + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  children[5u + pos_len] = term_new(0, TAG_NUM, 0, combo);
  s->trace[s->n_trace] = atp_trace_node(s, proof_only, TRACE_CP, children,
                                        6u + pos_len);
  u32 idx = s->n_trace;
  if (s->trace_pack) s->trace_eq_off[idx] = eq_off;
  s->n_trace++;
  if (proof_only)
    s->trace_x_bytes += (s->trace_eq_len - eq0) + 2u * sizeof(u64);
  atp_hx_trace_stamp(s, idx);
  return idx;
}

// When a CP_TRACE_LEAN'd CP survives to become a rule/equation, restore its
// TRACE_CP entry's children 2/3 (planted as sentinels at push) to the real
// term pair, so the WL proof builder -- which reads children 2/3 for the
// CriticalPairLemma statement -- renders a valid, verifiable proof.  Only the
// ~n_rules proof-relevant CPs are re-materialized here; the millions of
// joinable CPs that never enter a proof keep their sentinels.  The pair
// stored is the added rule's PERSISTENT terms (GC-rooted via s->lhs/s->rhs),
// never the transient pop-time scratch, so no dangling across a heap reset.
// No-op unless cp_trace_lean (the push planted real terms otherwise).
static void atp_trace_cp_rematerialize(AtpState *s, u32 tid, Term lhs, Term rhs) {
  if (s == NULL || !s->cp_trace_lean) return;
  if (tid == ATP_TRACE_NONE || tid >= s->n_trace) return;
  Term old = s->trace[tid];
  if (term_tag(old) != TAG_CTR || term_ext(old) != TRACE_CP) return;
  u32 n = term_ctr_n(old);
  if (n < 6u) return;
  u8 pos_len = (u8)term_val(term_ctr_at(old, 4));
  if ((u32)(6u + pos_len) != n) return;             // layout guard
  Term children[6 + CP_MAX_DEPTH];
  children[0] = term_ctr_at(old, 0);                // p_a
  children[1] = term_ctr_at(old, 1);                // p_b
  children[2] = lhs;                                // real terms now
  children[3] = rhs;
  children[4] = term_ctr_at(old, 4);                // pos_len
  for (u8 k = 0; k < pos_len; k++) children[5 + k] = term_ctr_at(old, 5 + k);
  children[5u + pos_len] = term_ctr_at(old, 5u + pos_len);  // combo
  s->trace[tid] = term_new_ctr(TRACE_CP, children, 6u + pos_len);
  if (tid >= ATP_MAX_TRACE) s->trace_x_heap = 1u;
}

// WM-distinguished-face flag (dist_rhs) for a newly-added unorientable
// equation selected from the CP queue at trace `src_trace`.
//
// WM's distinguished face is `selRec.lhs = KPLinks` (RUndEVerwaltung.c:
// 413, RE_ErzeugteGleichung, kept through BO_TermpaarNormieren with no
// side swap).  For a genuine two-parent superposition KPLinks =
// sigma(TP_RechteSeite(Vater)) (Unifikation1.c:916) -- the OUTER rule's
// RHS -- which thvm's CP constructor lands on cp.rhs, so WM's
// distinguished face = thvm's stored RHS: the dist_rhs=1 default.
//
// The exception is a CP that re-derives an EXISTING fact through a
// SINGLE parent (WM `ue (-X, 0)`: actualParent = -X real, otherParent
// absent).  There is no outer-rule-RHS role, so KPLinks keeps the pair's
// natural left side, which thvm also stores as cp.lhs: WM's distinguished
// face = thvm's stored LHS, dist_rhs=0.  This returns 0 ONLY for that
// single-real-parent CP; an initial axiom (WM `ue (0, 0)`, both parents
// absent, src = TRACE_AXIOM) and a genuine two-parent superposition both
// keep the dist_rhs=1 default.  The MeredithAnd/OrAssociativity pick-72
// divergence was exactly the single-parent case: WM's E-number-3 (the
// re-derived axiom 2, parents (-1, 0)) emits its CP batch from the
// natural LHS face (phase A first), but the dist_rhs=1 default flipped
// phases A<->D, sorting the partner-6 phase-A CP ahead of the partner-9
// phase-D CP and inverting their FIFO ages.
// Base relationship (no use_cp_side swap applied): is WM's
// distinguished face (KPLinks) thvm's natural stored RHS?  1 = KPLinks
// = sigma(r_i) on thvm's cp.rhs (two-parent superposition / axiom);
// 0 = KPLinks on thvm's cp.lhs (single-parent re-derivation).
static int atp_lr_sortieren_rec(Term lhs, Term rhs);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static int atp_lr_sortieren_rec_ft(const struct AtpFtCell *lhs,
                                   const struct AtpFtCell *rhs);
#endif

static u8 atp_wmo_eq_dist_rhs_base(const AtpState *s, u32 src_trace) {
  if (s == NULL) return 1u;
  // A proof-only entry (past the default cap) is absent to the search.
  if (src_trace == ATP_TRACE_NONE || src_trace >= s->n_trace ||
      src_trace >= ATP_MAX_TRACE) {
    // Past the proof-trace soft cap the trace entry is gone, but the
    // popped CP's origin class survives on the queue (cp_kind ->
    // last_popped_kind).  Apply the SAME class rule as the tagged
    // branches below: a superposition CP stores WM's (KPLinks,
    // KPRechts) AND a SIMPLIFY re-derivation keeps the natural left
    // side -- distinguished face = stored LHS (0) for both.  An axiom
    // never outlives the cap (pushed at init); it keeps the historical
    // default.  (WolframAxioms OrAssociativity fact 318: n_trace
    // crosses the 131072 cap near fact 300, so the trace-less default
    // walked the B/E sweep faces reversed and flipped the fact-533
    // weight tie; fact 1531, a trace-less SIMPLIFY, needed the same
    // stored-LHS verdict as the traced branch -- the A-vs-D root-twin
    // formation-order flip behind the fact-1549 kept-E side swap.)
    return (s->last_popped_kind == (u8)TRACE_CP ||
            s->last_popped_kind == (u8)TRACE_SIMPLIFY) ? 0u : 1u;
  }
  Term e = s->trace[src_trace];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) return 1u;
  u32 reason = term_ext(e);
  u32 p_a = (u32)term_val(term_ctr_at(e, 0));
  u32 p_b = (u32)term_val(term_ctr_at(e, 1));
  // A TRACE_SIMPLIFY victim (an interreduced fact re-queued as its
  // reduced equation) is WM's `ue (-X, 0)` re-derivation with a single
  // real parent (the dropped fact); a TRACE_CP with the otherParent
  // absent is the same shape from the superposition lane.  Both keep
  // the natural left side as the distinguished face -> dist_rhs=0.  A
  // genuine two-parent superposition takes the dist_rhs=1 default.
  if (reason == TRACE_SIMPLIFY) {
    // An IR-victim re-derivation keeps the natural left side as the
    // distinguished face, same as the header comment's single-parent
    // rule (MeredithAxioms AndAssociativity fact 31, a SIMPLIFY-kept E:
    // WM forms the fact-34 tie CP from the stored-LHS pattern first;
    // dist=1 walked thvm's B-phase on the stored RHS and flipped the
    // 34/36 weight tie).  A briefly-live dist=1 variant here was gated
    // only by the loose pre-RULEADD-anchored checker -- a phantom.
    return 0u;
  }
  if (reason == TRACE_CP) {
    // Post-4df4f2ab the stored raw pair is WM's (KPLinks, KPRechts): the
    // distinguished face is the stored LHS for every superposition CP.
    return 0u;
  }
  // INITIAL AXIOM: WM stores an input equation LRSortieren-canonicalised
  // (SpezNormierung.c :517-534) and U1_KPsBildenZuGleichung sweeps its
  // stored LinkeSeite first (A/B on l, then D/E on r -- no per-face
  // adjustment; Unifikation1.c :1596/:1637).  So WM's distinguished face
  // is thvm's stored RHS exactly when LRSortieren would SWAP thvm's
  // stored pair.  The former blanket dist_rhs=1 walked the wrong face
  // first whenever thvm's stored orientation already matched WM's
  // canonical one: ShefferAxioms OrAssociativity eq -2 (axiom 3,
  // (x1.(x2.x3))^2 # ((x2.x2).x1).((x3.x3).x1), stored A#B by BOTH
  // engines) had its activation sweep B-side subterms {L.1.1, L.2.1}
  // where WM's WM_CPFORMDUMP walks A-side {L.1.2, L.2.2} -- the CPNr/
  // FIFO-age fork behind the fact-52 divergence.
  if (reason == TRACE_AXIOM && term_ctr_n(e) >= 4u) {
    Term al = term_ctr_at(e, 2), ar = term_ctr_at(e, 3);
    return (atp_lr_sortieren_rec(al, ar) > 0) ? 1u : 0u;
  }
  return 1u;
}

// Overlap face-combo index (0..3) of the CP at `src_trace`, or 0xff if
// the trace entry is not a combo-tagged TRACE_CP (an axiom, a simplify,
// or a CP from a non-WM-order path).  Decoded from the last child of the
// TRACE_CP record (atp_trace_push_cp stores it at index 5 + pos_len).
static u8 atp_wmo_cp_combo(const AtpState *s, u32 src_trace) {
  if (s == NULL || src_trace == ATP_TRACE_NONE || src_trace >= s->n_trace) {
    return 0xffu;
  }
  Term e = s->trace[src_trace];
  if (term_tag(e) != TAG_CTR || term_ext(e) != TRACE_CP) return 0xffu;
  u32 n = term_ctr_n(e);
  if (n < 6u) return 0xffu;                 // no pos_len/combo tail
  u32 pos_len = (u32)term_val(term_ctr_at(e, 4));
  u32 combo_idx = 5u + pos_len;
  if (combo_idx >= n) return 0xffu;         // malformed; stay neutral
  return (u8)term_val(term_ctr_at(e, combo_idx));
}

// Does the CP-formation side geometry swap (use_cp_side) physically
// fire for the unorientable equation popped at `src_trace`?  PARENT-
// OVERLAP-AWARE, mirroring WM's reduct assignment (Unifikation1.c:916,
// KPLinks = sigma(TP_RechteSeite(Vater))): which face WM stores
// sigma(r_Vater)-left depends on the overlap geometry, so the swap is
// gated on the CP's face-combo index (tagged at formation, carried on the
// TRACE_CP record) rather than applied to every KBO_UN equation.  Only the
// reversed-i-face combos (2/3 in atp_overlap_ij -- where the overlap
// produced thvm's reduct-on-lhs vs WM's sigma(r)-on-lhs) need the swap;
// combos 0/1 already match WM's stored geometry, so swapping them
// mis-times the equation's own batch (the FIFO-age fork behind the
// blanket-swap CombinatorAxioms__BCKWToSKI__c2 regression).  This was
// established empirically against the WM `-a 4` reference: the combo>=2
// gate keeps the soa Sheffer OrAssociativity prefix at 124 AND, when on,
// keeps McCune/Hillman/Huntington/CommRing/AbelianTarski byte-identical
// (the old blanket reorder forked McCune).
static int atp_lr_sortieren_rec(Term lhs, Term rhs);

static u8 atp_cp_wm_side_swaps(const AtpState *s, u32 src_trace,
                               Term lhs, Term rhs) {
  if (s == NULL || !s->use_cp_side) return 0u;
  u8 combo = atp_wmo_cp_combo(s, src_trace);
  if (combo != 0xffu) return (combo >= 2u) ? 1u : 0u;  // reversed-i face only
  // Untagged (combo == 0xff): an INITIAL AXIOM has no superposition Vater,
  // so WM's KPLinks = sigma(r_Vater) CP-side geometry never applies -- WM
  // canonicalises an initial equation by LRSortieren (SpezNormierung.c:
  // 517-534), not the CP-formation swap.  The default bench leaves
  // use_lr_sortieren OFF, so mirror WM here by side-canonicalising an
  // axiom with LRSortieren.  NOTE: this reproduces the axiom swap soa's
  // Sheffer C-shape needs (soa prefix 124), but does NOT reconcile the
  // BCKWToSKI__c2 axiom -- that theorem needs the SAME unorientable axiom
  // NOT swapped (a downstream FIFO-age cascade forks selection at pick 55
  // whichever stored order WM and thvm agree on), an irreducible tension
  // that is why use_cp_side defaults OFF in the bench (see the bench
  // gate).  A TRACE_SIMPLIFY re-derivation or a non-WM-order superposition
  // CP keeps the legacy blanket swap.
  u32 reason = 99u;
  if (src_trace != ATP_TRACE_NONE && src_trace < s->n_trace) {
    Term e = s->trace[src_trace];
    if (term_tag(e) == TAG_CTR) reason = term_ext(e);
  }
  if (reason == TRACE_AXIOM) {
    return (atp_lr_sortieren_rec(lhs, rhs) > 0) ? 1u : 0u;
  }
  return 1u;
}

static u8 atp_wmo_eq_dist_rhs_dbg(const AtpState *s, u32 src_trace, u8 verdict) {
  static int on = -1;
  if (on < 0) on = getenv("THVM_ATP_DIST_TRACE") ? 1 : 0;
  if (on) {
    u32 reason = 999u;
    if (src_trace != ATP_TRACE_NONE && src_trace < s->n_trace) {
      Term e = s->trace[src_trace];
      if (term_tag(e) == TAG_CTR) reason = term_ext(e);
    }
    fprintf(stderr,
            "DISTREG trace=%u reason=%u kind=%u dist=%u n_rules=%u "
            "n_trace=%u swapped=%u\n",
            src_trace, reason, s->last_popped_kind, verdict, s->n_rules,
            s->n_trace, s->last_cp_wm_side_swapped);
  }
  return verdict;
}

static u8 atp_wmo_eq_dist_rhs(const AtpState *s, u32 src_trace) {
  // The flag answers: is WM's distinguished face (KPLinks) thvm's stored
  // RHS?  The physical CP_WM_SIDE swap reorders an unorientable equation
  // to (cp_rhs, cp_lhs) when it fires -- that moves KPLinks from thvm's
  // natural rhs (base == 1) onto the stored lhs, so the flag flips.  The
  // orient site records whether the swap actually fired in
  // last_cp_wm_side_swapped (the single source of truth -- the axiom
  // case's decision depends on the pre-swap term order, not recoverable
  // here from the stored sides), so read it back rather than recompute.
  u8 base = atp_wmo_eq_dist_rhs_base(s, src_trace);
  if (s != NULL && s->last_cp_wm_side_swapped) {
    return atp_wmo_eq_dist_rhs_dbg(s, src_trace, (u8)(1u - base));
  }
  return atp_wmo_eq_dist_rhs_dbg(s, src_trace, base);
}

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Also records a TRACE_AXIOM
// entry so the proof trace (stage 6.1) can identify this CP's
// origin downstream.  The CP queue is growable, so this never
// rejects for being full; returns 1 on success, 0 only on NULL
// state or a sort-check rejection.
// Shared body: sort-check, var-normalize, push a trace entry with
// the given `reason` / `parent_a`, and enqueue the equation as a CP.
// thvm_atp_add_equation enqueues an input axiom (TRACE_AXIOM, no
// parent); the interreduce re-queue path uses TRACE_SIMPLIFY with
// the dropped rule's trace index so the proof DAG stays connected.
static u8 atp_cp_perm_subsumed(Term lhs, Term rhs);

// Consume-once flag: the next atp_enqueue_equation push is the WM raw
// (untreated, size >= KPBehandeln gate) class -- bypasses the auto-
// MaxWeight stash exactly like the overlap raw class.  Set only by
// atp_wm_demote_drain; cleared on every enqueue.
static u8 g_enq_raw_untreated = 0u;

// s2a: forward-declared so the enqueue can snapshot axiom symbol masks
// (definition is next to thvm_atp_set_goals, far below).
static void atp_collect_symbols(Term t, u64 *mask);

static u8 atp_enqueue_equation(AtpState *s, Term lhs, Term rhs,
                               u32 reason, u32 parent_a) {
  // Consume the raw-untreated hint at ENTRY (the drop gates below early-
  // return; a leaked hint would mis-tag an unrelated later push).
  u8 enq_raw = g_enq_raw_untreated;
  g_enq_raw_untreated = 0u;
  // Consume the goal-derived seed the same way: the heap insert below
  // stamps cp_gd from g_cp_gd (a stale overlap-push value must not
  // leak onto an axiom/requeued equation, and vice versa).
  g_cp_gd = g_enq_gd_pending;
  g_enq_gd_pending = 0u;
  // Consume the formation-depth seed identically: axioms enqueue at 0,
  // interreduce requeues at their victim's r_depth (armed at the call
  // sites).  Without the reset a stale overlap-batch depth leaks onto
  // this equation's cp_depth / TWEE score.
  g_cp_formation_depth = g_enq_depth_pending;
  g_enq_depth_pending = 0u;
  if (s == NULL) return 0;
  static int enq_dbg = -1;
  if (enq_dbg < 0) enq_dbg = getenv("THVM_ATP_ENQ_DEBUG") != NULL;
  if (enq_dbg) {
    fprintf(stderr,
        "[enq] reason=%u  lhs:t%u/e%u",
        reason, term_tag(lhs), term_ext(lhs));
    if (term_tag(lhs) == TAG_CTR) {
      u32 n = term_ctr_n(lhs);
      fprintf(stderr, "/n%u", n);
      for (u32 c = 0; c < n && c < 3; c++) {
        Term ch = term_ctr_at(lhs, c);
        fprintf(stderr, " c%u(t%u/e%u)", c, term_tag(ch), term_ext(ch));
      }
    }
    fprintf(stderr, "  rhs:t%u/e%u",
        term_tag(rhs), term_ext(rhs));
    if (term_tag(rhs) == TAG_CTR) {
      u32 n = term_ctr_n(rhs);
      fprintf(stderr, "/n%u", n);
      for (u32 c = 0; c < n && c < 3; c++) {
        Term ch = term_ctr_at(rhs, c);
        fprintf(stderr, " c%u(t%u/e%u)", c, term_tag(ch), term_ext(ch));
      }
    }
    fprintf(stderr, "\n");
  }
  // 8.4d: when a WaldSpec is attached, reject ill-sorted inputs
  // before mutating state.  Each side must be well-sorted AND
  // both sides must share the same sort (an equation l = r in
  // a sorted signature requires `sort(l) == sort(r)`).
  // Homogeneous-mode (NULL spec or n_sorts == 0) returns sort 0
  // from wald_term_sort unconditionally so the gate is a no-op.
  if (s->spec != NULL) {
    u32 sl, sr;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    const AtpFtCell *bsl = atp_lazy_span_of(lhs);
    const AtpFtCell *bsr = atp_lazy_span_of(rhs);
    if (bsl != NULL && bsr != NULL) {
      sl = wald_term_sort_ft(s->spec, bsl);
      sr = wald_term_sort_ft(s->spec, bsr);
    } else
#endif
    {
      sl = wald_term_sort(s->spec, lhs);
      sr = wald_term_sort(s->spec, rhs);
    }
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
#ifdef ATP_VAR_NORM
  // 7c: an equation enters the engine as a queued CP.  Canonicalize
  // its variables here so the very first CP, like every later-
  // derived one, carries a dense [0, k) variable set.
  thvm_normalize_vars(&lhs, &rhs);
#endif
  // s2a: snapshot the input axiom's symbol mask for the SInE D-relation.
  // Axioms only (parent_a == ATP_TRACE_NONE) -- derived/requeued
  // equations are not part of the intake signature.  Gathered when s2a
  // drives selection OR when the goal_closeness node feature needs the
  // level table (graph export / GNN re-rank); the default engine leaves
  // this array untouched, so its step path stays byte-identical.
  if ((s->s2a_coeff > 0u || s->record_cp_graphs || s->gnn_rerank_period > 0u)
      && parent_a == ATP_TRACE_NONE) {
    u64 m = 0;
    atp_collect_symbols(lhs, &m);
    atp_collect_symbols(rhs, &m);
    if (s->n_s2a_ax >= s->cap_s2a_ax) {
      u32 nc = s->cap_s2a_ax ? s->cap_s2a_ax * 2u : 64u;
      u64 *nm = (u64 *)realloc(s->s2a_ax_mask, (size_t)nc * sizeof(u64));
      if (nm != NULL) { s->s2a_ax_mask = nm; s->cap_s2a_ax = nc; }
    }
    if (s->n_s2a_ax < s->cap_s2a_ax) s->s2a_ax_mask[s->n_s2a_ax++] = m;
  }
  // Permutation-subsumption (WM GZ_ACVerzichtbar): same filter as the
  // overlap-CP push.  Catches commutativity-shaped equations re-queued
  // by interreduce / RHS-composition (atp_add_equation_simplified)
  // before they enter the queue.  Skip for axioms (parent_a ==
  // ATP_TRACE_NONE) -- those are user-supplied and we must not drop.
  // AC-mask path perm-subsumes only at SELECTION (see KPV_Select port);
  // the legacy binary env path (no mask) keeps the enqueue-time drop.
  if (s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull
      && parent_a != ATP_TRACE_NONE
      && atp_cp_perm_subsumed(lhs, rhs)) {
    s->n_cps_dropped_perm_subsumed++;
    return 0;
  }
  // AC soundness gate for derived equations (skip user axioms).
  // Mirrors the CP-push path's gate at atp_overlap_ij's heap-push
  // site (see comment there).  Catches the simplify (TRACE_SIMPLIFY)
  // re-enqueue path where interreduce produces an ill-formed
  // equation by rewriting a rule's side via a degenerate rule that
  // shouldn't have been there in the first place.
#ifdef THVM_ATP_AC
  if (parent_a != ATP_TRACE_NONE && thvm_atp_get_ac_mask() != 0ull) {
    u8 lhs_is_var = (term_tag(lhs) == TAG_FVR);
    u8 rhs_is_var = (term_tag(rhs) == TAG_FVR);
    u8 fwd_sound = atp_vars_contained(rhs, lhs) && !lhs_is_var;
    u8 rev_sound = atp_vars_contained(lhs, rhs) && !rhs_is_var;
    if (!fwd_sound && !rev_sound) {
      return 0;
    }
  }
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Span-native requeue (the drain's bracket): canonicalize CLONES of
  // the parked spans, lean-trace and fused-pack straight from them --
  // the queued CP is bytes + trace + the armed weigh state, so no Term
  // materializes.  Conditions mirror the classic path's active gates:
  // the mask-0 perm drop and the AC soundness gate (both Term-side)
  // must be off-route, the trace must be packing (lean children), and
  // the var cliff guards the renumber.  Anything else falls through to
  // the classic Term arm (the caller decoded real Terms then).
  const AtpFtCell *enq_l = g_enq_ft_l;
  const AtpFtCell *enq_r = g_enq_ft_r;
  g_enq_ft_l = NULL;
  g_enq_ft_r = NULL;
  if (enq_l != NULL && enq_r != NULL && atp_lazy_rules_on() &&
      s->trace_pack && atp_order_is_plain_kbo(s) &&
      !(s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull &&
        parent_a != ATP_TRACE_NONE) &&
      ft_distinct_var_count(enq_l, enq_r) < REWRITE_MAX_VAR) {
    AtpFt *ea = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *cl = ft_clone_span(ea, enq_l, 0);
    AtpFtCell *cr = ft_clone_span(ea, enq_r, 0);
    ft_normalize_vars_pair(cl, cr);
    // Lean trace: sentinel children; the eq pair packs off the
    // canonical spans.
    g_trace_eq_ft_l = cl;
    g_trace_eq_ft_r = cr;
    u32 t_idx = atp_trace_push(s, reason, parent_a,
                               ATP_TRACE_NONE, lhs, rhs);
    // Fused pack + weigh state from the canonical spans (identity
    // renumber -- the emit's appearance order IS the canonical order),
    // then the push's fused arm consumes bytes/counts/weigh with no
    // Term reads, exactly like the walk former's routed pushes.
    AcpFuse *fz = &g_acp_fuse;
    fz->valid   = 0u;
    fz->twee_valid = 0u;
    fz->map.n   = 0u;
    fz->phidiff = 0;
    u32 pos = 0u, nodes = 0u, fpos = 0u;
    u8 ok = vcp_ft_emit_side(cl, NULL, NULL, NULL, s->kbo, &fz->map,
                             &pos, &nodes, fz, +1, &fpos);
    u32 lhs_nodes_ft = nodes, flh = fpos;
    if (ok) {
      ok = vcp_ft_emit_side(cr, NULL, NULL, NULL, s->kbo, &fz->map,
                            &pos, &nodes, fz, -1, &fpos);
    }
    if (ok) {
      fz->fl_n  = flh;
      fz->fr_n  = fpos - flh;
      fz->valid = 1u;
      g_vcp_emit.len       = pos;
      g_vcp_emit.nodes     = nodes;
      g_vcp_emit.lhs_nodes = lhs_nodes_ft;
      g_vcp_emit.armed     = 1u;
      u8 saved_fused = g_cp_push_fused;
      g_cp_push_fused = 1u;
      u8 is_ult = (reason == TRACE_AXIOM && parent_a == ATP_TRACE_NONE)
                      ? 1u : 0u;
      g_cp_push_kind = (u8)reason;
      atp_cp_heap_push(s, 0, 0, t_idx, is_ult, enq_raw);
      g_cp_push_kind  = (u8)TRACE_CP;
      g_cp_push_fused = saved_fused;
      g_vcp_emit.armed = 0u;
      g_acp_fuse.valid = 0u;
      return 1;
    }
    // Emit anomaly: fall through to the classic arm with REAL Terms --
    // decode from the parked spans.  The spans are the AUTHORITATIVE
    // content (the drain's NF); the buffered Term may be a sentinel OR
    // the stale pre-normalization pair, so decode whenever the span's
    // content differs (ft_eq_term is read-only and rejects sentinels).
    g_acp_fuse.valid = 0u;
    if (!ft_eq_term(enq_l, lhs)) lhs = ft_to_term(enq_l);
    if (!ft_eq_term(enq_r, rhs)) rhs = ft_to_term(enq_r);
#ifdef ATP_VAR_NORM
    thvm_normalize_vars(&lhs, &rhs);
#endif
    // trace already pushed (t_idx) -- reuse it for the classic push.
    u8 is_ult2 = (reason == TRACE_AXIOM && parent_a == ATP_TRACE_NONE)
                     ? 1u : 0u;
    g_cp_push_kind = (u8)reason;
    atp_cp_heap_push(s, lhs, rhs, t_idx, is_ult2, enq_raw);
    g_cp_push_kind = (u8)TRACE_CP;
    return 1;
  }
  // Fused arm declined (route conditions, e.g. the eager kill switch
  // atp_lazy_rules_on()==0): the parked spans are still the drain's
  // AUTHORITATIVE NF content.  The buffered Terms may be the stale
  // pre-normalization pair (real Terms on the eager path -- decoding
  // only TAG_NUM sentinels here requeued the UN-normalized victim and
  // drifted the eager trajectory from the lazy default: OA
  // WolframAxioms firstdiv=635, 1680 vs 1671 rules), so decode
  // whenever the span content differs -- byte-equal to the
  // pre-span-native drain, which decoded changed sides itself.
  if (enq_l != NULL && !ft_eq_term(enq_l, lhs)) lhs = ft_to_term(enq_l);
  if (enq_r != NULL && !ft_eq_term(enq_r, rhs)) rhs = ft_to_term(enq_r);
#endif
  u32 trace_idx = atp_trace_push(s, reason, parent_a,
                                 ATP_TRACE_NONE, lhs, rhs);
  // Waldmeister history-driven Act_ultimate (NewClassification.c:314,
  // DEF=1 block `initial = ultimate`): tag input axioms so they pop
  // before any derived CP regardless of heuristic weight.  Effective
  // only when s->use_initial_ultimate is set; off by default.
  u8 is_ultimate = (reason == TRACE_AXIOM && parent_a == ATP_TRACE_NONE)
                     ? 1u : 0u;
  // enq_raw: set only by the IR-victim drain for victims whose pair size
  // >= the KPBehandeln gate, i.e. WM's raw class (KPV_IROpferBehandeln's
  // lohntSichBehandlung skips treatment for them and recentCPinsert keeps
  // them in the SUE heap at raw weight -- WM never defers a KilledRE
  // victim, so the auto-MaxWeight stash must not swallow them here either;
  // the OA -auto fact-1378 class: the w1=2141 rule-487 victim sat in
  // thvm's stash while WM's FIFO popped it).
  // Stamp the origin class onto the queued CP (survives the proof-trace
  // soft cap; see cp_kind).  All other push sites are superposition CPs
  // and keep the TRACE_CP default.
  g_cp_push_kind = (u8)reason;
  atp_cp_heap_push(s, lhs, rhs, trace_idx, is_ultimate, enq_raw);
  g_cp_push_kind = (u8)TRACE_CP;
  return 1;
}

// --- ifeq (encoded-clause dispatcher) guard-aware CP selection --------
//
// See the AtpState.ifeq_syms doc block (thvm.h) for the mechanism.
// Detection runs here at axiom intake; the weight scale itself is
// applied in atp_cp_priority_sized_ex (Term route: every re-score
// path) and atp_cp_heap_insert_packed (FT-routed pushes whose Term
// args are sentinels).

// THVM_ATP_IFEQ (default on): the detector kill switch.
static int atp_ifeq_env_on(void) {
  static int on = -1;
  if (on < 0) {
    const char *e = getenv("THVM_ATP_IFEQ");
    on = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return on;
}
// THVM_ATP_IFEQ_GUARD (default 0 = mechanism disarmed).
static int atp_ifeq_guard_env(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_GUARD");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
  }
  return v;
}
// THVM_ATP_IFEQ_PENALTY (default 8): per-undecided-guard weight factor.
static u32 atp_ifeq_penalty(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_PENALTY");
    v = (e != NULL) ? atoi(e) : 8;
    if (v < 2) v = 2;
  }
  return (u32)v;
}

// THVM_ATP_IFEQ_DECIDE: float multiplier applied to a guard-DECIDING
// CP's priority (see the AtpState.use_ifeq_decide doc block, thvm.h).
// Unset or an explicit "0" leaves the mechanism DISARMED (base
// unchanged -- the default, byte-identical engine).  The bare-flag
// CLI default ("1", from `--ifeq-decide` with no `=value`) maps to
// the recommended 0.25 (4x cheaper); any other value is used directly
// as the multiplier, clamped to (0,1].  Cached fixed-point x1000 once
// so the hot CP-scoring path does no floating point.
static u32 atp_ifeq_decide_x1000(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_DECIDE");
    double d;
    if (e == NULL || e[0] == '\0') d = 0.0;
    else if (strcmp(e, "1") == 0)  d = 0.25;
    else                           d = strtod(e, NULL);
    if (d < 0.0) d = 0.0;
    if (d > 1.0) d = 1.0;
    v = (int)(d * 1000.0 + 0.5);
  }
  return (u32)v;
}
static u8 atp_ifeq_decide_armed(void) {
  return atp_ifeq_decide_x1000() > 0u;
}

// Dispatcher axiom shape: l = f(A,A,B[,C]) with every argument a
// variable, args 1,2 the SAME variable, B (and C) distinct from it,
// and r the bare variable B.
static u8 atp_ifeq_axiom_shape(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_FVR) return 0;
  u32 n = term_ctr_n(l);
  if (n != 3u && n != 4u) return 0;
  Term a0 = term_ctr_at(l, 0), a1 = term_ctr_at(l, 1);
  Term a2 = term_ctr_at(l, 2);
  if (term_tag(a0) != TAG_FVR || term_tag(a1) != TAG_FVR ||
      term_tag(a2) != TAG_FVR) return 0;
  if (term_ext(a0) != term_ext(a1)) return 0;
  if (term_ext(a2) == term_ext(a0) || term_ext(a2) != term_ext(r)) return 0;
  if (n == 4u) {
    Term a3 = term_ctr_at(l, 3);
    if (term_tag(a3) != TAG_FVR) return 0;
    if (term_ext(a3) == term_ext(a0) || term_ext(a3) == term_ext(a2))
      return 0;
  }
  return 1;
}

static void atp_ifeq_detect(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || !atp_ifeq_env_on()) return;
  Term f = 0;
  if      (atp_ifeq_axiom_shape(lhs, rhs)) f = lhs;
  else if (atp_ifeq_axiom_shape(rhs, lhs)) f = rhs;
  if (f == 0) return;
  u32 sym = term_ext(f);
  u32 cap = (u32)(sizeof s->ifeq_syms / sizeof s->ifeq_syms[0]);
  for (u32 i = 0; i < s->n_ifeq_syms; i++)
    if (s->ifeq_syms[i] == sym) return;
  if (s->n_ifeq_syms >= cap) return;
  s->ifeq_syms[s->n_ifeq_syms++] = sym;
  if (atp_ifeq_guard_env() >= 1) s->use_ifeq_guard = 1u;
  if (atp_ifeq_decide_armed()) s->use_ifeq_decide = 1u;
  if (getenv("THVM_ATP_IFEQ_DEBUG") != NULL) {
    fprintf(stderr, "[ifeq] dispatcher sym=%u detected (n=%u armed=%u)\n",
            sym, s->n_ifeq_syms, (u32)s->use_ifeq_guard);
  }
}

static u8 atp_ifeq_sym_is(const AtpState *s, u32 sym) {
  for (u32 i = 0; i < s->n_ifeq_syms; i++)
    if (s->ifeq_syms[i] == sym) return 1u;
  return 0u;
}

// Syntactic Term equality (vars by id, constructors by label + kids).
// Used only for the arg1-vs-arg2 decidedness test on detected ifeq
// nodes; early-exits on the first mismatch (guards differ at the head
// in practice: theorem(...) vs true).
static u8 atp_ifeq_term_eq(Term a, Term b) {
  typedef struct { Term a; Term b; } EqPair;
  EqPair  st_inl[ATP_WALK_INL];
  EqPair *st  = st_inl;
  u32     nst = 0u;
  u32     cst = ATP_WALK_INL;
  u8      eq  = 1u;
  for (;;) {
    u8 ta = term_tag(a), tb = term_tag(b);
    if (ta != tb) {
      eq = 0u;
    } else if (ta == TAG_FVR) {
      eq = (u8)(term_ext(a) == term_ext(b));
    } else if (ta != TAG_CTR) {
      eq = (u8)(a == b);
    } else if (term_ext(a) != term_ext(b)) {
      eq = 0u;
    } else {
      u32 n = term_ctr_n(a);
      if (n != term_ctr_n(b)) {
        eq = 0u;
      } else {
        while (nst + n > cst) {
          st = (EqPair *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) {
          st[nst].a = term_ctr_at(a, i);
          st[nst].b = term_ctr_at(b, i);
          nst++;
        }
      }
    }
    if (!eq || nst == 0u) {
      break;
    }
    nst--;
    a = st[nst].a;
    b = st[nst].b;
  }
  term_stack_release(st, st_inl);
  return eq;
}

// Count UNDECIDED guards: detected ifeq nodes whose args 1,2 are not
// syntactically equal (a decided one collapses at the next normalize
// via the dispatcher rule itself, so it must NOT be penalized).
static u32 atp_ifeq_guard_count(const AtpState *s, Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  u32   c   = 0u;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      u32 n = term_ctr_n(t);
      if ((n == 3u || n == 4u) && atp_ifeq_sym_is(s, term_ext(t)) &&
          !atp_ifeq_term_eq(term_ctr_at(t, 0), term_ctr_at(t, 1))) c++;
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return c;
}

// Fact test: in the ifeq encoding every semantically loaded derived
// equation has an ATOMIC side (`g = true`, a definitional fold to a
// constant, or a collapse residue that is a bare variable).  A CP with
// two compound sides is theory algebra (or/not identity churn on
// LCL306-10) that can never decide a guard -- it takes one extra
// penalty step (the mission's guard-deciding BONUS, expressed
// order-independently so re-scores agree).
static u8 atp_ifeq_side_atomic(Term t) {
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) return 1u;
  return (tg == TAG_CTR && term_ctr_n(t) == 0u) ? 1u : 0u;
}

// Scale a CP priority by penalty^guards, saturating below the
// ATP_CP_PRI_NEVER sentinel (a penalized CP still outranks a
// deprioritized-never one).
static u32 atp_ifeq_scale_pri(u32 base, u32 guards) {
  if (guards == 0u) return base;
  if (guards > 8u) guards = 8u;
  u64 b = (base == 0u) ? 1u : (u64)base;
  u64 p = (u64)atp_ifeq_penalty();
  for (u32 i = 0; i < guards; i++) {
    b *= p;
    if (b >= (u64)ATP_CP_PRI_NEVER) return ATP_CP_PRI_NEVER - 1u;
  }
  return (u32)b;
}

// Distinct-variable bitmask (var ids mod 64 -- an over-approximation
// dedupe, exact for the engine's REWRITE_MAX_VAR-bounded rule space).
static void atp_ifeq_var_mask(Term t, u64 *mask) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    u8 tg = term_tag(t);
    if (tg == TAG_FVR) {
      *mask |= 1ull << (term_ext(t) & 63u);
    } else if (tg == TAG_CTR) {
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}
// THVM_ATP_IFEQ_VARCAP (default 3): distinct-variable budget; every
// variable beyond it costs one penalty step.  The oracle proofs'
// general lemmas carry <= 3 distinct variables -- the ever-more-general
// 4-5-var theorem-schema towers (the classic LCL forward-chaining
// flood, cf. the horn-cd lane's --horn-cd-vars) are pure churn.
static u32 atp_ifeq_varcap(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_VARCAP");
    v = (e != NULL) ? atoi(e) : 3;
    if (v < 1) v = 1;
  }
  return (u32)v;
}

// THVM_ATP_IFEQ_FREE (default 1): guards within this budget are not
// penalized.  The oracle derivations run on facts AND one-guard
// conditional lemmas (partially discharged modus ponens: f14631/f105383
// and a dozen siblings in LCL306-10's refutation) -- penalizing the
// first guard buries that whole lemma class behind the fact churn.
static u32 atp_ifeq_guard_free(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_FREE");
    v = (e != NULL) ? atoi(e) : 1;
    if (v < 0) v = 0;
  }
  return (u32)v;
}

// Combined exponent for one CP: undecided guards on both sides beyond
// the free budget, plus one extra step when NEITHER side is atomic
// (see atp_ifeq_side_atomic), plus one step per distinct variable
// beyond the var budget.
static u32 atp_ifeq_weight_scale(const AtpState *s, Term lhs, Term rhs,
                                 u32 base) {
  u32 g = atp_ifeq_guard_count(s, lhs) + atp_ifeq_guard_count(s, rhs);
  u32 fr = atp_ifeq_guard_free();
  g = (g > fr) ? g - fr : 0u;
  if (!atp_ifeq_side_atomic(lhs) && !atp_ifeq_side_atomic(rhs)) g += 1u;
  u64 vm = 0ull;
  atp_ifeq_var_mask(lhs, &vm);
  atp_ifeq_var_mask(rhs, &vm);
  u32 nv = (u32)__builtin_popcountll(vm);
  u32 cap = atp_ifeq_varcap();
  if (nv > cap) g += nv - cap;
  return atp_ifeq_scale_pri(base, g);
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin of atp_ifeq_guard_count, for the walk former's routed
// pushes whose Term args are sentinels (the real content lives in the
// g_vcp_treat NF spans).  Same verdict: prefix walk, one tick per
// detected ifeq cell whose arg-1/arg-2 spans differ (ft_eq).
static u32 atp_ifeq_guard_count_ft(const AtpState *s, const AtpFtCell *t) {
  if (t == NULL) return 0u;
  u32 c = 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    if (!ft_is_var(x) && (x->arity == 3u || x->arity == 4u) &&
        atp_ifeq_sym_is(s, ft_ctr_sym(x))) {
      const AtpFtCell *a0 = x->next;
      const AtpFtCell *a1 = a0->end->next;
      if (!ft_eq(a0, a1)) c++;
    }
    if (x == stop) break;
  }
  return c;
}
// FT-span twin of atp_ifeq_weight_scale (guards + non-fact step +
// var-budget steps), for the routed pushes.
static u32 atp_ifeq_weight_scale_ft(const AtpState *s, const AtpFtCell *fl,
                                    const AtpFtCell *fr, u32 base) {
  u32 g = atp_ifeq_guard_count_ft(s, fl) + atp_ifeq_guard_count_ft(s, fr);
  u32 fr_budget = atp_ifeq_guard_free();
  g = (g > fr_budget) ? g - fr_budget : 0u;
  u8 fl_atomic = ft_is_var(fl) || fl->arity == 0u;
  u8 fr_atomic = ft_is_var(fr) || fr->arity == 0u;
  if (!fl_atomic && !fr_atomic) g += 1u;
  u64 vm = 0ull;
  for (int side = 0; side < 2; side++) {
    const AtpFtCell *t = side ? fr : fl;
    if (t == NULL) continue;
    const AtpFtCell *stop = t->end;
    for (const AtpFtCell *x = t; ; x = x->next) {
      if (ft_is_var(x)) vm |= 1ull << (ft_var_id(x) & 63u);
      if (x == stop) break;
    }
  }
  u32 nv = (u32)__builtin_popcountll(vm);
  u32 cap = atp_ifeq_varcap();
  if (nv > cap) g += nv - cap;
  return atp_ifeq_scale_pri(base, g);
}
#endif

// --- ifeq guard-DECIDING CP boost --------------------------------------
//
// The guard-count PENALTY above deprioritizes CPs that RETAIN
// undecided guards.  This is the complementary discipline: PRIORITIZE
// CPs that DECIDE one -- a CP of shape `g = true` where `g` matches a
// condition subterm some ACTIVE rule (or the goal) uses as an ifeq
// guard.  Deciding such a `g` lets the dispatcher axiom collapse that
// guard site on its very next normalize -- exactly the inference
// Vampire's literal selection performs when it resolves a guard
// literal instead of chasing the whole clause.
//
// Registry: guard condition subterms (arg0 of a detected-ifeq
// application whose arg0 != arg1 syntactically -- an UNDECIDED guard
// at registration time) are collected into a 4096-bucket fingerprint
// membership set (AtpState.ifeq_guard_mask) as they're seen in
// freshly added rules (atp_push_rule) and goal conjuncts
// (thvm_atp_set_goals).  The fingerprint is an ALPHA-INVARIANT
// structural hash (first-occurrence variable numbering, like
// thvm_normalize_vars but scoped to the single extracted subterm)
// folding in the root label and the full shape via the same FNV
// recurrence ft_hash/atp_term_struct_hash use (ft.c:672-718), so two
// occurrences of "the same" guard schema under different rules' local
// variable numbering -- e.g. sum(Y,X,V1) in one distributivity axiom
// and sum(A,B,C) in another -- fingerprint identically, while an
// unrelated term collides only by genuine hash accident.  A collision
// is harmless (a mis-boosted CP is simply selected a little early --
// nothing is dropped, completeness untouched); a stale entry left by
// a dead rule costs the same nothing-worse-than-early-pick, so
// removal on rule death is skipped.  The bucket count IS the cap
// (4096, the mission's ~4k budget) -- past it the false-positive rate
// rises smoothly, no hard ceiling to enforce.
//
// The "true" comparison constant is derived the same way: the first
// ATOMIC (0-ary) arg1 seen at any detected-ifeq site is cached on
// AtpState.ifeq_true_term and reused for an O(1) exact compare (no
// need to fingerprint the decided side, only the guard side).

// Bounded first-occurrence variable remap: canonicalizes a subterm's
// variable ids to 0,1,2,... in left-to-right preorder -- scoped to a
// single extracted guard subterm so two independent occurrences of
// the same schema (different rules, different raw var ids)
// canonicalize identically.  Overflow (never hit in practice: guards
// are small predicate atoms) shares one bucket via the sentinel id.
typedef struct { u32 raw[REWRITE_MAX_VAR]; u32 n; } AtpIfeqVarMap;
static u32 atp_ifeq_canon_var(AtpIfeqVarMap *vm, u32 raw_id) {
  for (u32 i = 0; i < vm->n; i++) if (vm->raw[i] == raw_id) return i;
  if (vm->n < REWRITE_MAX_VAR) { vm->raw[vm->n] = raw_id; return vm->n++; }
  return 0xFFFFu;
}
// FNV-style recurrence, same constants as ft_hash/atp_term_struct_hash;
// the only deviation is the canonical (not raw) variable id, which is
// what makes this hash alpha-invariant instead of alpha-exact.
static u64 atp_ifeq_guard_hash_rec(Term t, AtpIfeqVarMap *vm, u32 depth) {
  if (depth > 48u) return 0x9e3779b97f4a7c15ull;
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) {
    u32 cid = atp_ifeq_canon_var(vm, term_ext(t));
    return 0xfacefacefaceull ^ ((u64)cid * 0x100000001b3ull);
  }
  if (tg != TAG_CTR) return 0xdeadbeefcafebabeull ^ ((u64)t * 0x100000001b3ull);
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) n = 0u;
  u64 h = 0xcbf29ce484222325ull ^ ((u64)term_ext(t) * 0x100000001b3ull);
  for (u32 i = 0; i < n; i++) {
    h ^= atp_ifeq_guard_hash_rec(term_ctr_at(t, i), vm, depth + 1u);
    h *= 0x100000001b3ull;
  }
  return h ^ ((u64)n * 0x9e3779b97f4a7c15ull);
}
static u32 atp_ifeq_guard_bucket(Term t) {
  AtpIfeqVarMap vm; vm.n = 0u;
  u64 h = atp_ifeq_guard_hash_rec(t, &vm, 0u);
  return (u32)(h & 4095ull);
}
static void atp_ifeq_guard_mark(AtpState *s, Term g) {
  u32 b = atp_ifeq_guard_bucket(g);
  s->ifeq_guard_mask[(b >> 6) & 63u] |= 1ull << (b & 63u);
}
static u8 atp_ifeq_guard_hit(const AtpState *s, Term g) {
  u32 b = atp_ifeq_guard_bucket(g);
  return (u8)((s->ifeq_guard_mask[(b >> 6) & 63u] >> (b & 63u)) & 1ull);
}

// Telemetry (env-gated print in the bench stats block): how many CP
// scoring events actually applied the boost, and how many boosted CPs
// were subsequently popped by thvm_atp_select_cp -- see
// atp_ifeq_decide_scale / atp_ifeq_decide_hit below.  Zero for the
// entire run whenever the mechanism is disarmed (the default).
u64 g_ifeq_decide_n_hit    = 0;  // scoring events that applied the boost
u64 g_ifeq_decide_n_popped = 0;  // popped CPs that were guard-deciding

// Walk a freshly-added rule (or goal conjunct) for detected-ifeq
// applications with an UNDECIDED guard (arg0 != arg1); register arg0
// as a live guard fingerprint and, on the first atomic arg1 seen,
// cache it as the encoding's boolean-true constant.  Depth/arity
// bounded like every other recursive term walk in this file.
static void atp_ifeq_guard_register_rec(AtpState *s, Term t, u32 depth) {
  if (depth > 48u || term_tag(t) != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return;
  if ((n == 3u || n == 4u) && atp_ifeq_sym_is(s, term_ext(t))) {
    Term a0 = term_ctr_at(t, 0), a1 = term_ctr_at(t, 1);
    if (!atp_ifeq_term_eq(a0, a1)) {
      atp_ifeq_guard_mark(s, a0);
      if (s->ifeq_true_term == 0 && term_tag(a1) == TAG_CTR &&
          term_ctr_n(a1) == 0u) {
        s->ifeq_true_term = a1;
      }
    }
  }
  for (u32 i = 0; i < n; i++)
    atp_ifeq_guard_register_rec(s, term_ctr_at(t, i), depth + 1u);
}
static void atp_ifeq_guard_register(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || !s->use_ifeq_decide) return;
  atp_ifeq_guard_register_rec(s, lhs, 0u);
  atp_ifeq_guard_register_rec(s, rhs, 0u);
}

// True iff {lhs,rhs} is exactly {a registered guard, the true
// constant}, order-independent (equations are unoriented at CP
// formation).  Shared by the Term-route scale below and the
// thvm_atp_select_cp pop-time telemetry.
static u8 atp_ifeq_decide_hit(const AtpState *s, Term lhs, Term rhs) {
  if (s->ifeq_true_term == 0) return 0u;
  if (atp_ifeq_term_eq(rhs, s->ifeq_true_term) &&
      atp_ifeq_guard_hit(s, lhs)) return 1u;
  if (atp_ifeq_term_eq(lhs, s->ifeq_true_term) &&
      atp_ifeq_guard_hit(s, rhs)) return 1u;
  return 0u;
}
// Priority scale: a single-shot multiplier (not a per-guard
// exponential like the penalty -- one guard decided is one guard
// decided) by THVM_ATP_IFEQ_DECIDE when atp_ifeq_decide_hit fires.
// Floors at 1 so a boosted CP never collapses to the always-first
// sentinel 0; never touches an already-NEVER priority.
static u32 atp_ifeq_decide_scale(const AtpState *s, Term lhs, Term rhs,
                                 u32 base) {
  if (base == 0u || base >= ATP_CP_PRI_NEVER) return base;
  if (!atp_ifeq_decide_hit(s, lhs, rhs)) return base;
  g_ifeq_decide_n_hit++;
  u64 b = (u64)base * atp_ifeq_decide_x1000() / 1000ull;
  return (u32)(b == 0ull ? 1ull : b);
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin of atp_ifeq_guard_hash_rec/atp_ifeq_guard_bucket, for
// matching a routed-push CP's NF spans against the SAME registry the
// Term-route populates (ft_ctr_sym/term_ext share one label space
// engine-wide -- see the ft_hash / atp_term_struct_hash mirror this
// walk copies the recurrence shape from -- so the two hashes agree on
// shared content by construction).
static u64 atp_ifeq_guard_hash_ft_rec(const AtpFtCell *t, AtpIfeqVarMap *vm) {
  // Folded post-order over an explicit stack; variables are
  // canonicalized as they are reached, in the recursion's preorder.
  typedef struct { const AtpFtCell *child; u32 left; u32 n; u64 h; } GhFrame;
  GhFrame  fr_inl[ATP_WALK_INL];
  GhFrame *fr  = fr_inl;
  u32      nfr = 0u;
  u32      cfr = ATP_WALK_INL;
  u64      v   = 0u;
  for (;;) {
    u8 leaf = 1u;
    if (ft_is_var(t)) {
      u32 cid = atp_ifeq_canon_var(vm, ft_var_id(t));
      v = 0xfacefacefaceull ^ ((u64)cid * 0x100000001b3ull);
    } else {
      u32 n = t->arity;
      u64 h = 0xcbf29ce484222325ull ^ ((u64)ft_ctr_sym(t) * 0x100000001b3ull);
      if (n == 0u) {
        v = h;
      } else {
        if (nfr == cfr) {
          fr = (GhFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].child = t->next;
        fr[nfr].left  = n;
        fr[nfr].n     = n;
        fr[nfr].h     = h;
        nfr++;
        leaf = 0u;
      }
    }
    if (leaf) {
      while (nfr > 0u) {
        GhFrame *f = &fr[nfr - 1u];
        f->h ^= v;
        f->h *= 0x100000001b3ull;
        if (f->left > 0u) break;
        v = f->h ^ ((u64)f->n * 0x9e3779b97f4a7c15ull);
        nfr--;
      }
      if (nfr == 0u) {
        break;
      }
    }
    GhFrame *f = &fr[nfr - 1u];
    t        = f->child;
    f->child = t->end->next;
    f->left--;
  }
  term_stack_release(fr, fr_inl);
  return v;
}
static u32 atp_ifeq_guard_bucket_ft(const AtpFtCell *t) {
  AtpIfeqVarMap vm; vm.n = 0u;
  u64 h = atp_ifeq_guard_hash_ft_rec(t, &vm);
  return (u32)(h & 4095ull);
}
static u8 atp_ifeq_guard_hit_ft(const AtpState *s, const AtpFtCell *t) {
  u32 b = atp_ifeq_guard_bucket_ft(t);
  return (u8)((s->ifeq_guard_mask[(b >> 6) & 63u] >> (b & 63u)) & 1ull);
}
// FT-span twin of atp_ifeq_decide_scale, for the routed pushes whose
// Term args are sentinels (see atp_ifeq_weight_scale_ft above).
static u32 atp_ifeq_decide_scale_ft(const AtpState *s, const AtpFtCell *fl,
                                    const AtpFtCell *fr, u32 base) {
  if (s->ifeq_true_term == 0 || base == 0u || base >= ATP_CP_PRI_NEVER)
    return base;
  u8 hit = 0u;
  if (ft_eq_term(fr, s->ifeq_true_term) &&
      atp_ifeq_guard_hit_ft(s, fl)) hit = 1u;
  else if (ft_eq_term(fl, s->ifeq_true_term) &&
           atp_ifeq_guard_hit_ft(s, fr)) hit = 1u;
  if (!hit) return base;
  g_ifeq_decide_n_hit++;
  u64 b = (u64)base * atp_ifeq_decide_x1000() / 1000ull;
  return (u32)(b == 0ull ? 1ull : b);
}
#endif

// --- ifeq formation-time resolved-guard stripping ---------------------
//
// THVM_ATP_IFEQ_STRIP (+ CLI --ifeq-strip): under --lazy-normalize the
// raw >= KPBehandeln-gate class is queued UNTREATED and scored on its
// raw size, so a guard-DECIDING superposition -- whose conclusion
// carries ifeq(t, t, A, B) subterms that one dispatcher rewrite
// collapses to A -- is buried at guard-tower weight while its treated
// (< gate) siblings score their normal forms.  When the dispatcher
// detector armed (AtpState.ifeq_syms), strip every RESOLVED guard from
// the raw class at CP formation: a single deterministic bottom-up
// rewrite with the dispatcher axiom (sound -- the axiom is an input
// equation; the dispatcher rule system is orthogonal, so the collapse
// is confluent), done WITHOUT the full R-normalize the lazy gate
// exists to skip.  The queued/scored content becomes the stripped
// pair; the proof trace keeps the RAW overlap (exactly the treated
// class's contract -- the pop-time record normalize re-derives from
// the trace's raw pair, see the record_norm_steps block).  A CP whose
// stripped size falls BELOW the gate is reclassified into the treated
// class (full doR treatment + joined-drop), which is what WM would
// have done had the CP formed at that size.  Off (default): byte-
// identical engine.
//
// THVM_ATP_LAZY_SCORE_NF (+ CLI --lazy-score-nf): the scoring-only
// companion -- estimate the raw CP's normal-form size with a step-
// capped doR normalize on a THROWAWAY clone (value 1 = default cap of
// 4 rewrite steps; N >= 2 = cap N) and feed the estimated per-side
// counts to the CP priority in place of the raw counts.  The queued
// bytes/spans stay the raw pair (the lazy pipeline is untouched);
// selection just stops ranking guard towers by their tower size.

static int atp_ifeq_strip_env(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_STRIP");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
  }
  return v;
}
static u8 atp_ifeq_strip_armed(const AtpState *s) {
  return (s != NULL && s->use_lazy_normalize && s->n_ifeq_syms > 0u &&
          atp_ifeq_strip_env() >= 1) ? 1u : 0u;
}
// 0 = off; 1 = the default 4-step cap; N >= 2 = cap N.
static u32 atp_lazy_scorenf_cap(void) {
  static int v = -2;
  if (v == -2) {
    const char *e = getenv("THVM_ATP_LAZY_SCORE_NF");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
  }
  return (v == 1) ? 4u : (u32)v;
}

u64 g_ifeq_strip_n_hit     = 0;  // raw CPs with >= 1 resolved guard stripped
u64 g_ifeq_strip_n_reclass = 0;  // stripped below the gate -> treated class
u64 g_scorenf_n            = 0;  // raw CPs scored via the capped-NF probe
u64 g_scorenf_shrunk       = 0;  // probe size strictly below the raw size

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static inline void atp_ft_mirror_ensure(AtpState *s);

// Does the span contain a RESOLVED detected-dispatcher guard --
// an ifeq cell whose arg-1/arg-2 spans are syntactically identical?
// (The strip's trigger: with zero resolved guards no bottom-up
// collapse can fire, including the cascade case, whose innermost
// collapse is itself a directly-resolved guard.)
static u8 atp_ifeq_span_has_resolved(const AtpState *s, const AtpFtCell *t) {
  if (t == NULL) return 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    if (!ft_is_var(x) && (x->arity == 3u || x->arity == 4u) &&
        atp_ifeq_sym_is(s, ft_ctr_sym(x))) {
      const AtpFtCell *a0 = x->next;
      const AtpFtCell *a1 = a0->end->next;
      if (ft_eq(a0, a1)) return 1u;
    }
    if (x == stop) break;
  }
  return 0u;
}

// Preorder cell count of a span == the pair's symbol-count nodes (one
// cell per constructor / variable; the NUM marker's 3 cells overcount
// a Term-side 1, acceptable -- the count feeds selection weights and
// the gate reclass, never an exactness contract).
static u32 atp_ifeq_span_cells(const AtpFtCell *t) {
  u32 n = 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    n++;
    if (x == stop) break;
  }
  return n;
}

// Rebuild `c` with every resolved detected-dispatcher guard collapsed
// to its then-branch.  Bottom-up (children stripped BEFORE the parent
// guard equality test), so nested collapses cascade to the fixpoint in
// one pass -- the leftmost-innermost normal form of the orthogonal
// dispatcher rule system.  Returns a fresh Arena-A span (caller frees
// the original); NULL on an arity/depth anomaly (caller keeps the
// original raw pair -- the strip is best-effort selection repair).
static AtpFtCell *atp_ifeq_strip_build(const AtpState *s, AtpFt *a,
                                       const AtpFtCell *c, u32 depth) {
  enum { IFEQ_STRIP_MAX_DEPTH = 200, IFEQ_STRIP_MAX_KIDS = 16 };
  if (depth > (u32)IFEQ_STRIP_MAX_DEPTH) return NULL;
  if (ft_is_var(c)) return ftnew_var(a, ft_var_id(c), 0);
  if (ft_ctr_sym(c) == ATPFT_NUM_MARKER) return ft_clone_span(a, c, 0);
  u32 n = (u32)c->arity;
  if (n == 0u) return ftnew_const(a, ft_ctr_sym(c), 0);
  if (n > (u32)IFEQ_STRIP_MAX_KIDS) return NULL;
  AtpFtCell *kids[IFEQ_STRIP_MAX_KIDS];
  const AtpFtCell *k = c->next;
  for (u32 i = 0; i < n; i++) {
    kids[i] = atp_ifeq_strip_build(s, a, k, depth + 1u);
    if (kids[i] == NULL) {
      for (u32 j = 0; j < i; j++) ft_free_span(a, kids[j], kids[j]->end);
      return NULL;
    }
    k = k->end->next;
  }
  if ((n == 3u || n == 4u) && atp_ifeq_sym_is(s, ft_ctr_sym(c)) &&
      ft_eq(kids[0], kids[1])) {
    AtpFtCell *keep = kids[2];
    for (u32 i = 0; i < n; i++) {
      if (i != 2u) ft_free_span(a, kids[i], kids[i]->end);
    }
    return keep;
  }
  return ftnew_ctr(a, ft_ctr_sym(c), (u16)n, kids, 0);
}

// Strip both sides of a stashed span pair in place (replace + free the
// originals).  Returns the stripped pair's combined cell count, or 0
// when nothing was stripped / an anomaly kept the originals.
static u32 atp_ifeq_strip_spans(const AtpState *s, AtpFt *a,
                                AtpFtCell **pfl, AtpFtCell **pfr) {
  if (*pfl == NULL || *pfr == NULL) return 0u;
  if (!atp_ifeq_span_has_resolved(s, *pfl) &&
      !atp_ifeq_span_has_resolved(s, *pfr)) {
    return 0u;
  }
  AtpFtCell *nl = atp_ifeq_strip_build(s, a, *pfl, 0u);
  if (nl == NULL) return 0u;
  AtpFtCell *nr = atp_ifeq_strip_build(s, a, *pfr, 0u);
  if (nr == NULL) {
    ft_free_span(a, nl, nl->end);
    return 0u;
  }
  ft_free_span(a, *pfl, (*pfl)->end);
  ft_free_span(a, *pfr, (*pfr)->end);
  *pfl = nl;
  *pfr = nr;
  return atp_ifeq_span_cells(nl) + atp_ifeq_span_cells(nr);
}

// Term-route strip (the classic build's raw class: real cp_lhs/cp_rhs
// Terms).  On a strip hit writes the stripped Terms back and returns
// the new combined node count; 0 = untouched.
static u32 atp_ifeq_strip_terms(AtpState *s, Term *lhs, Term *rhs) {
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = ft_from_term(a, *lhs, 0);
  AtpFtCell *fr = ft_from_term(a, *rhs, 0);
  u32 sz = atp_ifeq_strip_spans(s, a, &fl, &fr);
  if (sz != 0u) {
    *lhs = ft_to_term(fl);
    *rhs = ft_to_term(fr);
  }
  ft_free_span(a, fl, fl->end);
  ft_free_span(a, fr, fr->end);
  return sz;
}

// Emit-route strip (the raw >= gate class serialized straight to bytes
// by vcp_emit_fused / vcp_ft_emit_fused): decode the scratch bytes to
// spans, and on a strip hit CONVERT the push to the treated-stash
// route -- the stripped spans become the queue payload (ftp_pack_fused
// at the heap push re-packs + re-weighs them) while the emitted raw
// bytes stay in the scratch as the stash's eager raw-pair record
// (raw_len), exactly what the trace store consumes.  The caller owns
// the stash release after the push (the former's tail only disarms the
// emit).  Returns the stripped size (hijacked) or 0 (emit untouched).
static u32 atp_ifeq_strip_hijack_emit(AtpState *s) {
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  const u8 *pp = g_acp_scratch;
  AtpFtCell *fl = acp_unpack_term_ft(a, &pp, 0);
  AtpFtCell *fr = acp_unpack_term_ft(a, &pp, 0);
  u32 sz = atp_ifeq_strip_spans(s, a, &fl, &fr);
  if (sz == 0u) {
    ft_free_span(a, fl, fl->end);
    ft_free_span(a, fr, fr->end);
    return 0u;
  }
  g_vcp_treat.fl      = fl;
  g_vcp_treat.fr      = fr;
  g_vcp_treat.raw_len = g_vcp_emit.len;   // raw bytes already in scratch
  g_vcp_treat.lo      = NULL;             // no deferred-emit operands
  g_vcp_treat.sub     = NULL;
  g_vcp_treat.rin     = NULL;
  g_vcp_treat.ro      = NULL;
  g_vcp_treat.subst   = NULL;
  g_vcp_treat.armed   = 1u;
  g_vcp_emit.armed    = 0u;
  g_acp_fuse.valid    = 0u;   // the emit's weigh state died with it
  return sz;
}

// Fix-2 probe: per-side normal-form size ESTIMATE for a raw-class CP
// -- step-capped doR normalize on throwaway clones.  Returns 1 with
// *nl/*nr set; 0 = no source (caller keeps the raw counts).
static u8 atp_cp_lazy_scorenf_probe(AtpState *s, const u8 *packed,
                                    Term lhs, Term rhs, u32 cap,
                                    u32 *nl, u32 *nr) {
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = NULL, *fr = NULL;
  if (g_vcp_treat.armed && g_vcp_treat.fl != NULL &&
      g_vcp_treat.fr != NULL) {
    fl = ft_clone_span(a, g_vcp_treat.fl, 0);
    fr = ft_clone_span(a, g_vcp_treat.fr, 0);
  } else if (packed != NULL) {
    const u8 *pp = packed;
    fl = acp_unpack_term_ft(a, &pp, 0);
    fr = acp_unpack_term_ft(a, &pp, 0);
  } else if (lhs != 0 && rhs != 0 && term_tag(lhs) != TAG_NUM &&
             term_tag(rhs) != TAG_NUM) {
    fl = ft_from_term(a, lhs, 0);
    fr = ft_from_term(a, rhs, 0);
  }
  if (fl == NULL || fr == NULL) {
    if (fl != NULL) ft_free_span(a, fl, fl->end);
    if (fr != NULL) ft_free_span(a, fr, fr->end);
    return 0u;
  }
  fl = atp_rules_only_normalize_ft(s, fl, cap);
  fr = atp_rules_only_normalize_ft(s, fr, cap);
  *nl = atp_ifeq_span_cells(fl);
  *nr = atp_ifeq_span_cells(fr);
  ft_free_span(a, fl, fl->end);
  ft_free_span(a, fr, fr->end);
  return 1u;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// --- tgt-sel: goal-targeted CP-selection weights ----------------------
//
// A SELECTION-side companion to the tgt preprocessing port (Vampire's
// tgt=full/ground is a preprocessing transformation -- see
// pr_tgt_transform in tests/test_atp_wolfram_bench.c): per-symbol
// CP-selection weights derived from the GOAL, so goal-flavored terms
// are systematically lighter and their CPs float to the queue front.
// Symbols occurring in the conjecture (goal equation + the labels fed
// through thvm_atp_tgt_add_sym) weigh 1; every other symbol weighs
// THVM_ATP_TGT_NONGOAL (default 2).  Mode 2 ('ground') additionally
// raises variables to the non-goal weight, so ground goal-copies
// outrank general terms.  Applied as a multiplier on the CP priority:
//   base' = base * weighted_count / plain_count
// which IS the weighted symbol-count for the ADD weight mode and a
// proportional bias for every other mode (GT/TWEE/...).  The
// reduction-ordering KBO is untouched -- orientation stays
// soundness-clean; this is selection bias only, nothing is dropped,
// completeness untouched.  See AtpState.tgt_sym_mask (thvm.h).
// Applied in atp_cp_priority_sized_ex (Term route: every re-score
// path) and atp_cp_heap_insert_packed (FT-routed pushes whose Term
// args are sentinels), exactly like the ifeq guard scale above.

// THVM_ATP_TGT_SEL: 0 off (default), 1 'full', 2 'ground'.
static u32 atp_tgt_env(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_TGT_SEL");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
    if (v > 2) v = 2;
  }
  return (u32)v;
}
// THVM_ATP_TGT_NONGOAL (default 2): the non-goal symbol weight.
static u32 atp_tgt_nongoal(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_TGT_NONGOAL");
    v = (e != NULL) ? atoi(e) : 2;
    if (v < 2) v = 2;
  }
  return (u32)v;
}

#define ATP_TGT_MASK_WORDS 64u    // 4096 buckets; labels alias mod 4096
static u8 atp_tgt_sym_hit(const AtpState *s, u32 lbl) {
  return (u8)((s->tgt_sym_mask[(lbl >> 6) & (ATP_TGT_MASK_WORDS - 1u)]
               >> (lbl & 63u)) & 1ull);
}
static void atp_tgt_mask_collect(AtpState *s, Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      u32 lbl = term_ext(t);
      s->tgt_sym_mask[(lbl >> 6) & (ATP_TGT_MASK_WORDS - 1u)] |=
          1ull << (lbl & 63u);
      u32 n = term_ctr_n(t);
      if (n <= REWRITE_MAX_ARITY) {
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}
// Collect the goal-symbol bitset lazily (union over every conjunct,
// like SOS's goal_sym_mask, plus the externally-declared goal-cone
// labels); thvm_atp_set_goals resets tgt_ready.
static void atp_tgt_ensure(AtpState *s) {
  if (s->tgt_ready) return;
  memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
  if (s->n_goals > 1u) {
    for (u32 g = 0; g < s->n_goals; g++) {
      atp_tgt_mask_collect(s, s->goals_lhs[g]);
      atp_tgt_mask_collect(s, s->goals_rhs[g]);
    }
  } else {
    if (s->goal_lhs) atp_tgt_mask_collect(s, s->goal_lhs);
    if (s->goal_rhs) atp_tgt_mask_collect(s, s->goal_rhs);
  }
  for (u32 i = 0; i < ATP_TGT_MASK_WORDS; i++)
    s->tgt_sym_mask[i] |= s->tgt_ext_mask[i];
  s->tgt_ready = 1u;
}
static u8 atp_tgt_armed(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0 || atp_tgt_env() == 0u) return 0u;
  atp_tgt_ensure(s);
  return 1u;
}

// Externally declare a goal-cone symbol label for the tgt scheme.  The
// FindProof CLI feeds the TPTP negated-conjecture units' symbols here:
// Vampire derives its tgt weights from the WHOLE negated conjecture,
// and on the machine-equationalized '-10' class the final goal
// equation is a bare `a != b` while the conjecture's content lives in
// its sibling hypothesis units (without this the multiplier is nearly
// uniform across CPs -- no reordering).  ORed into the live mask at
// the next lazy collection; inert while THVM_ATP_TGT_SEL is off.
fn void thvm_atp_tgt_add_sym(AtpState *s, u32 label) {
  if (s == NULL) return;
  s->tgt_ext_mask[(label >> 6) & (ATP_TGT_MASK_WORDS - 1u)] |=
      1ull << (label & 63u);
  s->tgt_ready = 0u;
}

// Weighted/plain counts -> multiplicative bias on `base`, saturating
// below the ATP_CP_PRI_NEVER sentinel like the ifeq scale.  plain == 0
// (sentinel/empty Term args, the FT-routed pushes) leaves the base
// unchanged; the FT twin below applies the exact span counts instead.
static u32 atp_tgt_scale_counts(u32 base, u32 gc, u32 oc, u32 vc) {
  u64 plain = (u64)gc + oc + vc;
  if (plain == 0ull) return base;
  u64 w = (u64)atp_tgt_nongoal();
  u64 scaled = (u64)gc + w * oc + ((atp_tgt_env() >= 2u) ? w * vc : (u64)vc);
  u64 b = (u64)(base == 0u ? 1u : base) * scaled / plain;
  if (b >= (u64)ATP_CP_PRI_NEVER) return ATP_CP_PRI_NEVER - 1u;
  return (u32)b;
}
static void atp_tgt_count_rec(const AtpState *s, Term t,
                              u32 *gc, u32 *oc, u32 *vc) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    u8 tg = term_tag(t);
    if (tg == TAG_FVR) {
      (*vc)++;
    } else if (tg == TAG_CTR) {
      if (atp_tgt_sym_hit(s, term_ext(t))) (*gc)++; else (*oc)++;
      u32 n = term_ctr_n(t);
      if (n <= REWRITE_MAX_ARITY) {
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}
static u32 atp_tgt_weight_scale(const AtpState *s, Term lhs, Term rhs,
                                u32 base) {
  u32 gc = 0u, oc = 0u, vc = 0u;
  atp_tgt_count_rec(s, lhs, &gc, &oc, &vc);
  atp_tgt_count_rec(s, rhs, &gc, &oc, &vc);
  return atp_tgt_scale_counts(base, gc, oc, vc);
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin of atp_tgt_weight_scale, for the routed pushes whose
// Term args are sentinels (the real content lives in the g_vcp_treat
// NF spans).  Same walk shape as atp_ifeq_weight_scale_ft's.
static u32 atp_tgt_weight_scale_ft(const AtpState *s, const AtpFtCell *fl,
                                   const AtpFtCell *fr, u32 base) {
  u32 gc = 0u, oc = 0u, vc = 0u;
  for (int side = 0; side < 2; side++) {
    const AtpFtCell *t = side ? fr : fl;
    if (t == NULL) continue;
    const AtpFtCell *stop = t->end;
    for (const AtpFtCell *x = t; ; x = x->next) {
      if (ft_is_var(x)) vc++;
      else if (atp_tgt_sym_hit(s, ft_ctr_sym(x))) gc++;
      else oc++;
      if (x == stop) break;
    }
  }
  return atp_tgt_scale_counts(base, gc, oc, vc);
}
#endif

// --- spb=goal / nwc: derivation-level goal bias ------------------------
//
// Vampire's shared GRP loop-theory winner carries spb=goal (goal-derived
// clauses get selection priority) and nwc=2 (non-goal clauses' selection
// weight doubled).  The symbol-level twin (--tgt-sel) is landed above;
// this is the DERIVATION-level analog: a per-CP bit (cp_gd) seeded on
// the conjecture-derived input equations (goal-cone tgt definitions +
// conjecture-role units, via thvm_atp_mark_gd_next), propagated
// parent-OR-wise through CP formation (g_cp_gd, the g_cp_formation_depth
// discipline) and rule birth (r_gd).  When THVM_ATP_SPB_GOAL is armed,
// every NON-goal-derived CP's priority is scaled by THVM_ATP_NWC
// (float, default 2.0) -- goal-derived CPs float, everything else sinks
// proportionally.  Selection bias only: nothing is dropped, ordering
// untouched, byte-identical when unarmed.

// THVM_ATP_SPB_GOAL: 0 off (default), 1 armed.
static u8 atp_spb_goal_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_SPB_GOAL");
    v = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  return (u8)v;
}
// THVM_ATP_NWC (default 2.0): non-goal-derived CP weight multiplier,
// parsed once to fixed-point x16 (clamped to [1.0, 64.0]).
static u32 atp_nwc_x16(void) {
  static u32 v = 0u;
  if (v == 0u) {
    const char *e = getenv("THVM_ATP_NWC");
    double d = (e != NULL && e[0] != '\0') ? strtod(e, NULL) : 2.0;
    if (d < 1.0) d = 1.0;
    if (d > 64.0) d = 64.0;
    v = (u32)(d * 16.0 + 0.5);
    if (v < 16u) v = 16u;
  }
  return v;
}
// Priority scale: goal-derived (gd=1) CPs keep their base; everything
// else is multiplied by NWC.  Saturates below the never-select
// sentinel; NEVER-class priorities pass through untouched.
static u32 atp_spb_scale(u32 base, u8 gd) {
  if (gd || !atp_spb_goal_on() || base >= ATP_CP_PRI_NEVER) return base;
  u64 b = (u64)base * atp_nwc_x16() / 16u;
  if (b >= (u64)ATP_CP_PRI_NEVER) return ATP_CP_PRI_NEVER - 1u;
  return (u32)b;
}

// --- drc=off analog: keep demod-redundant folded lemmas ----------------
//
// Vampire's winner also runs drc=off (demodulation_redundancy_check):
// clauses that demodulation would discard as redundant are KEPT -- 25
// of its GRP678-1 proof steps unfold to x=x and 56 to axiom instances,
// i.e. the proof passes THROUGH lemmas completion join-discards.  Our
// discard sites are the push-time triviality/join gate and the pop-time
// join gate.  Cheapest faithful approximation: when THVM_ATP_DRC_OFF is
// armed and the fold band is registered (thvm_atp_set_fold_band, the
// sF*/sFlat* definitional symbols), a CP whose sides CARRY a folded
// symbol and differ syntactically is NOT dropped on a join verdict --
// the RAW (pre-normalize) pair is queued / oriented instead, so the
// folded-coherence rewrite content becomes rules.  Default off,
// byte-identical when unarmed.

// THVM_ATP_DRC_OFF: 0 off (default), 1 armed.
static u8 atp_drc_off_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_OFF");
    v = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  return (u8)v;
}
// THVM_ATP_DRC_GD (default 1): restrict the drc keep to GOAL-DERIVED
// CPs (the spb=goal derivation bit).  Under tgt=3 every CP carries a
// folded symbol -- the bare fold test is vacuous and the keep lane
// floods (GRP678-1: 3.0M keeps/60s); the conjecture-descended cone is
// the discriminating signal.  =0 reverts to the pure fold test.
static u8 atp_drc_gd_gate(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_GD");
    v = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return (u8)v;
}
// THVM_ATP_DRC_PUSH (default 1): also keep at the push-time join gate.
// =0 keeps only at the pop (selection) gate -- the site closest to
// Vampire's activation-time forward-demodulation retention.
static u8 atp_drc_push_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_PUSH");
    v = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return (u8)v;
}
// THVM_ATP_DRC_MAXW (default 16, 0 = uncapped): raw-pair node cap on
// the keep.  Even deduped, the folded-redundant class is huge (GRP678:
// 1.1M distinct keeps/60s); the oracle proof's pass-through lemmas are
// TINY folded identities (sF-layer compositions), so the keep targets
// that shape.
static u32 atp_drc_maxw(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_MAXW");
    v = (e != NULL && e[0] != '\0') ? atoi(e) : 16;
    if (v < 0) v = 0;
  }
  return (u32)v;
}
static void atp_symbol_count_upto(Term t, u32 cap, u32 *acc);
static u8 atp_drc_size_ok_term(Term l, Term r) {
  u32 cap = atp_drc_maxw();
  if (cap == 0u) return 1u;
  u32 n = 0u;
  atp_symbol_count_upto(l, cap + 1u, &n);
  if (n <= cap) atp_symbol_count_upto(r, cap + 1u, &n);
  return (u8)(n <= cap);
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static u8 atp_drc_size_ok_ft(const AtpFtCell *l, const AtpFtCell *r) {
  u32 cap = atp_drc_maxw();
  if (cap == 0u) return 1u;
  u32 n = 0u;
  for (int side = 0; side < 2; side++) {
    const AtpFtCell *t = side ? r : l;
    if (t == NULL) continue;
    const AtpFtCell *stop = t->end;
    for (const AtpFtCell *x = t; ; x = x->next) {
      if (++n > cap) return 0u;
      if (x == stop) break;
    }
  }
  return 1u;
}
#endif
static u8 atp_drc_armed(const AtpState *s) {
  return (u8)(s != NULL && s->fold_hi > s->fold_lo && atp_drc_off_on());
}
// Does the Term carry a folded-band symbol?
static u8 atp_term_has_fold(const AtpState *s, Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  u8    hit = 0u;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      u32 lbl = term_ext(t);
      if (lbl >= s->fold_lo && lbl < s->fold_hi) {
        hit = 1u;
        break;
      }
      u32 n = term_ctr_n(t);
      if (n <= REWRITE_MAX_ARITY) {
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin (prefix walk, the atp_tgt_weight_scale_ft shape).
static u8 atp_ft_has_fold(const AtpState *s, const AtpFtCell *t) {
  if (t == NULL) return 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    if (!ft_is_var(x)) {
      u32 lbl = ft_ctr_sym(x);
      if (lbl >= s->fold_lo && lbl < s->fold_hi) return 1u;
    }
    if (x == stop) break;
  }
  return 0u;
}
// drc keep stash for the FT-routed treated class: vcp_treat_joinable
// normalizes the raw stash cells IN PLACE, so a drc candidate's raw
// pair is cloned here BEFORE the normalize; the push gate swaps the
// clones back into the stash on a keep (or frees them on a survivor).
static AtpFtCell *g_drc_raw_l = NULL;
static AtpFtCell *g_drc_raw_r = NULL;
#endif

// Keep-lane dedup: without it the keep floods with RE-DERIVATIONS of
// the same folded lemma (GRP678-1: 1.9M keeps / <5k distinct rules in
// 60s).  Each unique raw-pair structural hash is kept at most once per
// problem (thvm_atp_set_fold_band resets the table); collisions and
// probe overflow err on the ALLOW side.  Hashes are NOT var-canonical
// -- alpha-variant re-derivations may slip through, which only costs a
// duplicate keep.  THVM_ATP_DRC_DEDUP=0 disables (the flood A/B).
#define ATP_DRC_SEEN_BITS 20u
static u64 g_drc_seen[1u << ATP_DRC_SEEN_BITS];
static u8 atp_drc_dedup_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_DEDUP");
    v = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return (u8)v;
}
// 1 = this lemma hash was already kept (suppress); 0 = fresh (insert).
static u8 atp_drc_seen_probe(u64 h) {
  if (!atp_drc_dedup_on()) return 0u;
  if (h == 0ull) h = 1ull;
  u32 m = (1u << ATP_DRC_SEEN_BITS) - 1u;
  u32 i = (u32)(h ^ (h >> 32)) & m;
  for (u32 k = 0; k < 64u; k++, i = (i + 1u) & m) {
    if (g_drc_seen[i] == h) return 1u;
    if (g_drc_seen[i] == 0ull) {
      g_drc_seen[i] = h;
      return 0u;
    }
  }
  return 0u;   // probe overflow: allow the keep
}
static u64 atp_drc_pair_hash_term(Term l, Term r) {
  return atp_term_struct_hash(l) * 0x9e3779b97f4a7c15ull
       ^ atp_term_struct_hash(r);
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static u64 atp_drc_pair_hash_ft(const AtpFtCell *l, const AtpFtCell *r) {
  return ft_hash(l) * 0x9e3779b97f4a7c15ull ^ ft_hash(r);
}
#endif

fn void thvm_atp_mark_gd_next(AtpState *s) {
  (void)s;
  g_enq_gd_pending = 1u;
}

fn void thvm_atp_set_fold_band(AtpState *s, u32 lo, u32 hi) {
  if (s == NULL) return;
  s->fold_lo = lo;
  s->fold_hi = hi;
  // Fresh problem intake: reset the drc keep-lane dedup table.
  memset(g_drc_seen, 0, sizeof g_drc_seen);
}

fn void thvm_atp_set_fold_defs(AtpState *s, const u8 *arity,
                               const Term *body, u32 n) {
  if (s == NULL) return;
  if (n > ATP_NEEDLE_MAX_DEFS) n = ATP_NEEDLE_MAX_DEFS;
  for (u32 i = 0; i < n; i++) {
    s->fold_def_arity[i] = arity[i];
    s->fold_def_body[i]  = body[i];
  }
}

fn void thvm_atp_set_orig_axioms(AtpState *s, const Term *lhs,
                                 const Term *rhs, u32 n) {
  if (s == NULL) return;
  if (n > ATP_NEEDLE_MAX_AXIOMS) n = ATP_NEEDLE_MAX_AXIOMS;
  for (u32 i = 0; i < n; i++) {
    s->needle_ax_l[i] = lhs[i];
    s->needle_ax_r[i] = rhs[i];
  }
  s->n_needle_ax = n;
}

// --- needle: narrow the drc=off keep set to instance/x=x shapes ------
//
// Vampire's GRP678-1 proof passes THROUGH ~80 folded-redundant lemmas
// (demodulation_redundancy_check keeps them); wholesale drc=off (above)
// keeps every folded-and-joins CP -- a haystack (GRP678: 1.1M distinct
// keeps/60s even deduped).  Empirically those ~80 lemmas' sF-UNFOLDING
// (substitute every fold-band symbol back to the raw pre-tgt subterm
// it names, recursively) is always one of two shapes: an alpha-instance
// of one of the ORIGINAL axioms, or a trivial (syntactically-equal
// after canonical renumbering) x=x.  The needle keeps ONLY those.
//
// THVM_ATP_DRC_NEEDLE: 0 off (default), 1 armed.  Only takes effect
// where THVM_ATP_DRC_OFF is already armed (needle strictly NARROWS
// that keep set); off is byte-identical to plain drc=off.
static u8 atp_drc_needle_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_NEEDLE");
    v = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  return (u8)v;
}
// THVM_ATP_DRC_NEEDLE_CAP (default 256, 0 = uncapped): node-count
// budget on the FULLY UNFOLDED pair (both sides combined).  Over-cap
// is a fail-closed drop (not kept) -- the oracle's pass-through lemmas
// are tiny folded identities, so a giant unfold is never the needle's
// target class, and bounding it keeps the per-candidate cost flat.
static u32 atp_drc_needle_cap(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_NEEDLE_CAP");
    v = (e != NULL && e[0] != '\0') ? atoi(e) : 256;
    if (v < 0) v = 0;
  }
  return (u32)v;
}

// Recursive unfold of a folded Term back to the original signature:
// bottom-up, substitute every fold-band root label with its registered
// definition body (the occurrence's own children bind the body's FVR
// 0..arity-1 placeholders via thvm_subst_apply).  Definition bodies are
// fold-free BY CONSTRUCTION (pr_tgt_transform's pr_tgt_subs[] are raw
// pre-transform subterms, never rewritten to mention fresher labels),
// so one substitution per occurrence fully resolves it; atp_term_has_fold
// on the result is a cheap defensive check, not a second unfold pass
// (a hit means a corrupt/mismatched def table, not a legitimate nested
// fold -- fails closed rather than looping).  *budget is a combined
// node-count ceiling shared across both CP sides (see
// atp_drc_needle_cap); each substitution's OWN result size is charged
// against it via atp_symbol_count_upto, so a substitution that blows up
// the term is caught even though thvm_subst_apply itself doesn't meter.
// Returns 0 on over-cap / a missing or arity-mismatched definition
// (the caller drops the candidate, same as if needle were never
// consulted); 1 with *out filled otherwise.
static u8 atp_drc_needle_unfold(const AtpState *s, Term t, u32 *budget,
                                Term *out) {
  if (*budget == 0u) return 0u;
  (*budget)--;
  if (term_tag(t) != TAG_CTR) { *out = t; return 1u; }
  u32 lbl = term_ext(t);
  u32 n   = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return 0u;
  Term kids[REWRITE_MAX_ARITY];
  u8 changed = 0u;
  for (u32 i = 0; i < n; i++) {
    Term ci = term_ctr_at(t, i);
    Term ko;
    if (!atp_drc_needle_unfold(s, ci, budget, &ko)) return 0u;
    if (ko != ci) changed = 1u;
    kids[i] = ko;
  }
  if (lbl >= s->fold_lo && lbl < s->fold_hi) {
    u32 di = lbl - s->fold_lo;
    Term def = (di < ATP_NEEDLE_MAX_DEFS) ? s->fold_def_body[di] : 0;
    if (def == 0 || s->fold_def_arity[di] != n) return 0u;
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    for (u32 i = 0; i < n; i++) sub.bindings[i] = kids[i];
    Term r = thvm_subst_apply(def, &sub);
    if (atp_term_has_fold(s, r)) return 0u;   // defensive; see above
    u32 rn = 0u;
    atp_symbol_count_upto(r, *budget + 1u, &rn);
    if (rn > *budget) return 0u;
    *budget -= rn;
    *out = r;
    return 1u;
  }
  *out = changed ? term_new_ctr(lbl, kids, n) : t;
  return 1u;
}

// (a) alpha-instance test: does the unfolded pair (ul, ur) match one of
// the registered pre-tgt axioms under a single one-way substitution,
// in either orientation ("axiom lhs->ul, axiom rhs->ur" or "axiom
// lhs->ur, axiom rhs->ul", the SAME subst across both sides of a try)?
static u8 atp_drc_needle_ax_match(const AtpState *s, Term ul, Term ur) {
  for (u32 i = 0; i < s->n_needle_ax; i++) {
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    if (thvm_match(s->needle_ax_l[i], ul, &sub) &&
        thvm_match(s->needle_ax_r[i], ur, &sub)) {
      return 1u;
    }
    memset(&sub, 0, sizeof sub);
    if (thvm_match(s->needle_ax_l[i], ur, &sub) &&
        thvm_match(s->needle_ax_r[i], ul, &sub)) {
      return 1u;
    }
  }
  return 0u;
}

// The needle gate: unfold both sides (shared budget), then keep iff
// (b) the unfolded pair is trivial (syntactically equal once
// canonically renumbered -- the general "x=x" shape) or (a) an
// axiom instance.  Telemetry counters record the verdict; over-cap
// unfolds count separately from a clean instance/x=x miss.
static u8 atp_drc_needle_ok(AtpState *s, Term l, Term r) {
  u32 cap = atp_drc_needle_cap();
  u32 budget = (cap == 0u) ? 0xFFFFFFFFu : cap;
  Term ul, ur;
  if (!atp_drc_needle_unfold(s, l, &budget, &ul) ||
      !atp_drc_needle_unfold(s, r, &budget, &ur)) {
    s->n_cps_drc_needle_overcap++;
    return 0u;
  }
  thvm_normalize_vars(&ul, &ur);
  if (kbo_eq(ul, ur) || atp_drc_needle_ax_match(s, ul, ur)) {
    s->n_cps_drc_needle_kept++;
    return 1u;
  }
  s->n_cps_drc_needle_dropped++;
  return 0u;
}

// Thin gate wrapper: needle off (the default) is a pure pass-through,
// so every drc=off call site composes it with one `&&` and pays
// nothing extra when THVM_ATP_DRC_NEEDLE is unset.
static u8 atp_drc_needle_gate(AtpState *s, Term l, Term r) {
  if (!atp_drc_needle_on()) return 1u;
  return atp_drc_needle_ok(s, l, r);
}

fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
  atp_ifeq_detect(s, lhs, rhs);
  return atp_enqueue_equation(s, lhs, rhs, TRACE_AXIOM, ATP_TRACE_NONE);
}

// Forward decls (defined further down the file).
static u8 atp_push_rule(AtpState *s, Term lhs, Term rhs);

// Install a pre-oriented axiom directly as a rewrite rule, bypassing
// the engine's KBO orientation step.  The caller asserts `lhs -> rhs`
// is the intended direction; the engine uses it as-is.  Used for the
// WL Rule[lhs, rhs] axiom surface (TFindProof[..., {a == b, c -> d}]).
//
// Semantics:
//   * No KBO compare.  atp_push_rule still writes r_orient[] based on
//     KBO, but we OVERRIDE to 1 below so ordered-rewrite + indexing
//     treat the rule as forward-only.
//   * Decrements n_unorient back if KBO would have classified it as
//     unorientable (keeps the unorient bookkeeping accurate).
//   * Generates CPs against existing rules immediately, mirroring the
//     post-orient_and_add CP-gen step at thvm_atp_step's tail.
//
// Soundness: forcing an orientation that violates KBO does NOT break
// soundness -- the rewrite `lhs -> rhs` is still a valid equational
// consequence.  But it CAN break completion's termination guarantee:
// a non-decreasing rule may loop the rewriter.  The variable-safety
// check (vars(rhs) subset of vars(lhs)) at the WL surface catches the
// outright-unsafe case (introducing fresh vars); the still-unsafe
// case of a non-terminating direction is the user's responsibility.
fn AtpAddedRange thvm_atp_install_oriented_rule(AtpState *s, Term lhs,
                                                Term rhs) {
  AtpAddedRange r = {0, 0, 0};
  if (s == NULL) return r;
  u32 idx = s->n_rules;
  if (!atp_push_rule(s, lhs, rhs)) return r;
  // atp_push_rule may have rejected as duplicate -- if n_rules didn't
  // bump, nothing to override.
  if (s->n_rules == idx) return r;
  u32 i = s->n_rules - 1u;
  if (!s->r_orient[i]) {
    s->r_orient[i] = 1u;
    if (s->n_unorient > 0u) s->n_unorient--;
    // r_orient flip changes the rewrite system: a rule previously
    // applied two-faces now applies only its lhs face.  Bump the
    // revision the IR cookie keys on.
    s->r_revision++;
  }
  // Trace lineage: the select/orient path stamps every rule's r_trace
  // with a TRACE_ORIENT entry whose parent chains back to the source
  // axiom.  Mirror that here -- without it r_trace[i] stays
  // ATP_TRACE_NONE and every CP overlapping this rule records a NONE
  // parent, which the WL proof lifter indexes as trace[[2^32]] and the
  // ProofObject collapses to $Failed.
  u32 ax_t = atp_trace_push(s, TRACE_AXIOM, ATP_TRACE_NONE,
                            ATP_TRACE_NONE, lhs, rhs);
  if (ax_t != ATP_TRACE_NONE) {
    u32 or_t = atp_trace_push(s, TRACE_ORIENT, ax_t, ATP_TRACE_NONE,
                              s->lhs[i], s->rhs[i]);
    if (or_t != ATP_TRACE_NONE) {
      s->r_trace[i] = or_t;
      atp_rj_content(s, i);
    }
  }
  if (s->use_emission_order) atp_wmo_insert_fact(s, i);
  r.first = idx;
  r.count = 1;
  // Generate CPs between this newly-installed rule and the rules
  // installed earlier in this init batch.  At step 0 of the engine
  // this catches all the pre-init oriented rules so the saturation
  // can later overlap them with the equation-axiom rules.
  thvm_atp_generate_cps(s, r);
  return r;
}


// Re-queue a simplified older rule (interreduce path).  Records a
// TRACE_SIMPLIFY entry whose parent_a is the dropped rule's trace
// index, so a proof consumer can replay the reduction chain instead
// of treating the equation as an unjustified axiom.
static u8 atp_add_equation_simplified(AtpState *s, Term lhs, Term rhs,
                                      u32 parent_trace) {
  // Goal-connected requeues record their interreduce-time reduction as
  // NORM_STEP chains threaded between the parent and the TRACE_SIMPLIFY
  // entry.  With whole-run RecordNorm off this hop is otherwise the only
  // unrecorded rewrite in a goal chain, and the WL lift cannot re-derive
  // ordered reductions by search (OrAssociativity goal-CP requeue).  The
  // re-derivation runs against the CURRENT R -- the same rule set that
  // just produced (lhs, rhs) -- so the recorded chain lands on the same
  // pair; on any mismatch fall back to the unchained entry.
  return atp_enqueue_equation(s, lhs, rhs, TRACE_SIMPLIFY, parent_trace);
}

// Walk `t` and bit-set every TAG_CTR label encountered into `*mask`.
// Used by thvm_atp_set_goal to populate s->conj_sym_mask for the
// ATP_CP_WEIGHT_CONJSYM weight mode.
static void atp_collect_symbols(Term t, u64 *mask) {
  Term stack[256];
  u32  sp = 0;
  if (sp < 256) stack[sp++] = t;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) == TAG_CTR) {
      u32 lab = term_ext(cur);
      if (lab < 64u) *mask |= ((u64)1 << lab);
      u32 n = term_ctr_n(cur);
      for (u32 i = 0; i < n && sp < 256; i++) {
        stack[sp++] = term_ctr_at(cur, i);
      }
    }
  }
}

// Release the multi-goal arrays (set_goals re-set / clear / free).
static void atp_goals_release(AtpState *s) {
  free(s->goals_lhs);    s->goals_lhs    = NULL;
  free(s->goals_rhs);    s->goals_rhs    = NULL;
  free(s->goals_lhs_nf); s->goals_lhs_nf = NULL;
  free(s->goals_rhs_nf); s->goals_rhs_nf = NULL;
#ifdef THVM_ATPFT_RULES
  free(s->goals_lhs_ft);  s->goals_lhs_ft  = NULL;
  free(s->goals_rhs_ft);  s->goals_rhs_ft  = NULL;
  free(s->goals_nf_ft_l); s->goals_nf_ft_l = NULL;
  free(s->goals_nf_ft_r); s->goals_nf_ft_r = NULL;
#endif
  s->n_goals = 0;
  s->goals_joined_mask = 0;
}

// Set the conjecture (single equation goal_lhs == goal_rhs).
// Calling with goal_lhs == 0 clears the goal (completion mode).
// Returns 1 on success, 0 if 8.4d's sort-check rejected the goal.
// s2a (Vampire sine_to_age): per-symbol SInE level via the Hoder-
// Voronkov D-relation, seeded from the conjecture symbols.  gen(s) =
// number of (live) rules containing s (the conjecture counts once, as in
// tp_sine_pass); a rule C is triggered by a REACHED symbol s iff
// gen(s) <= gt or gen(s) <= tol*min_gen(C), where min_gen(C) is the least
// generality over C's gt-general symbols when that least is still rare
// (<= rc), else the plain least over all of C's symbols -- exactly the
// tp_sine_pass CLI trigger.  A symbol first reached at round k gets level
// k+1 (goal symbols are level 0); symbols never reached stay REMOTE.  The
// BFS grows `reach` per round (proper shortest-path distance, order-
// independent), so a common hub symbol like `mult` does NOT drag every
// axiom to level 1 -- the division/inverse axioms whose only rare symbol
// stays unreachable end up REMOTE, exactly the slice SInE selection would
// drop.  Called from thvm_atp_set_goals only when s2a_coeff > 0, so the
// whole pass never runs for the byte-identical default engine.
static void atp_compute_sine_levels(AtpState *s) {
  for (u32 i = 0; i < WALD_MAX_SYMBOLS; i++)
    s->sym_slvl[i] = ATP_REL_LEVEL_REMOTE;
  if (s->goal_lhs == 0) return;
  u32 gt = 1u; double tol = 1.5; u32 rc = 64u;
  { const char *e = getenv("THVM_ATP_S2A_GT");
    if (e != NULL && e[0] != '\0') { long v = atol(e); gt = (v < 0) ? 0u : (u32)v; } }
  { const char *e = getenv("THVM_ATP_S2A_TOL");
    if (e != NULL && e[0] != '\0') { tol = atof(e); if (tol < 1.0) tol = 1.0; } }
  { const char *e = getenv("THVM_ATP_S2A_RC");
    if (e != NULL && e[0] != '\0') { long v = atol(e); rc = (v < 0) ? 0u : (u32)v; } }
  // Axiom source: the enqueue-time symbol-mask snapshot (the rule set is
  // still empty here -- input equations enter as queued CPs).
  const u64 *ax = s->s2a_ax_mask;
  u32 nax = s->n_s2a_ax;
  // Generality gen[s] = #(axioms + conjecture) containing s.
  u32 gen[WALD_MAX_SYMBOLS];
  for (u32 i = 0; i < WALD_MAX_SYMBOLS; i++) gen[i] = 0u;
  for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++)
    if (s->conj_sym_mask & ((u64)1 << lab)) gen[lab]++;
  for (u32 i = 0; i < nax; i++)
    for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++)
      if (ax[i] & ((u64)1 << lab)) gen[lab]++;
  // Seed: conjecture symbols at level 0.
  for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++)
    if (s->conj_sym_mask & ((u64)1 << lab)) s->sym_slvl[lab] = 0u;
  u64 reach = s->conj_sym_mask;
  for (u32 round = 0; round < WALD_MAX_SYMBOLS; round++) {
    u64 next = 0;
    for (u32 i = 0; i < nax; i++) {
      u64 m = ax[i];
      if (!(m & reach)) continue;              // no reached symbol: no trigger
      u32 min_all = 0xffffffffu, min_ex = 0xffffffffu;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
        if (!(m & ((u64)1 << lab))) continue;
        u32 g = gen[lab];
        if (g < min_all) min_all = g;
        if (g > gt && g < min_ex) min_ex = g;
      }
      u32 mingen = (min_ex <= rc) ? min_ex : min_all;
      u8 hit = 0;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS && !hit; lab++) {
        if (!(m & ((u64)1 << lab))) continue;
        if (!(reach & ((u64)1 << lab))) continue;
        if (gen[lab] <= gt || (double)gen[lab] <= tol * (double)mingen) hit = 1;
      }
      if (!hit) continue;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
        if (!(m & ((u64)1 << lab))) continue;
        if (s->sym_slvl[lab] == ATP_REL_LEVEL_REMOTE) {
          s->sym_slvl[lab] = (u8)(round + 1u);
          next |= ((u64)1 << lab);
        }
      }
    }
    if (!next) break;
    reach |= next;
  }
  {
    const char *dmp = getenv("THVM_ATP_S2A_DUMP");
    if (dmp != NULL && dmp[0] != '\0') {
      u32 hist[ATP_S2A_REMOTE_LVL + 2];
      for (u32 i = 0; i < ATP_S2A_REMOTE_LVL + 2; i++) hist[i] = 0u;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
        if (gen[lab] == 0u) continue;
        u32 lv = (s->sym_slvl[lab] == ATP_REL_LEVEL_REMOTE)
                   ? (ATP_S2A_REMOTE_LVL + 1u) : s->sym_slvl[lab];
        if (lv > ATP_S2A_REMOTE_LVL + 1u) lv = ATP_S2A_REMOTE_LVL + 1u;
        hist[lv]++;
        fprintf(stderr, "S2A sym lab=%u gen=%u level=%u\n",
                lab, gen[lab], s->sym_slvl[lab]);
      }
      fprintf(stderr, "S2A level histogram (coeff=%u):", s->s2a_coeff);
      for (u32 i = 0; i <= ATP_S2A_REMOTE_LVL + 1u; i++)
        if (hist[i]) fprintf(stderr, " L%u=%u%s", i, hist[i],
                             (i == ATP_S2A_REMOTE_LVL + 1u) ? "(REMOTE)" : "");
      fprintf(stderr, "\n");
    }
  }
}

// s2a: per-CP SInE level = max over the CP's TAG_CTR symbols of their
// sym_slvl, a REMOTE (goal-unreachable) symbol contributing
// ATP_S2A_REMOTE_LVL.  0 when s2a is off or no goal.  The FIFO/age
// selection adds s2a_coeff*this to the CP's cp_seq (goal-distant -> later).
static u32 atp_cp_sine_level(const AtpState *s, Term lhs, Term rhs) {
  if (s->s2a_coeff == 0u || s->goal_lhs == 0) return 0u;
  Term stack[256];
  u32 sp = 0;
  if (sp < 256) stack[sp++] = lhs;
  if (sp < 256) stack[sp++] = rhs;
  u32 best = 0u;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) == TAG_CTR) {
      u32 lab = term_ext(cur);
      u32 lvl = (lab < WALD_MAX_SYMBOLS &&
                 s->sym_slvl[lab] != ATP_REL_LEVEL_REMOTE)
                  ? (u32)s->sym_slvl[lab] : ATP_S2A_REMOTE_LVL;
      if (lvl > ATP_S2A_REMOTE_LVL) lvl = ATP_S2A_REMOTE_LVL;
      if (lvl > best) best = lvl;
      u32 n = term_ctr_n(cur);
      for (u32 i = 0; i < n && sp < 256; i++) stack[sp++] = term_ctr_at(cur, i);
    }
  }
  return best;
}

fn u8 thvm_atp_set_goal(AtpState *s, Term lhs, Term rhs) {
  if (s != NULL && lhs == 0) return thvm_atp_set_goals(s, NULL, NULL, 0);
  return thvm_atp_set_goals(s, &lhs, &rhs, 1);
}

// Set an n-goal conjunction: every pair (lhs[g], rhs[g]) must join for
// goal_check to report ATP_PROVED.  n == 0 clears the goal (completion
// mode); n == 1 is the single-conjecture case and behaves exactly as
// thvm_atp_set_goal always has.  The conjecture symbol mask and the
// relevance levels are computed over the UNION of all goals' symbols.
// Returns 1 on success, 0 on n > ATP_MAX_GOALS or any sort-check
// rejection (state unmodified on failure).
fn u8 thvm_atp_set_goals(AtpState *s, const Term *lhs, const Term *rhs,
                         u32 n) {
  if (s == NULL) return 0;
  if (n > ATP_MAX_GOALS) return 0;
  // Clearing the goal: n == 0 means "completion mode", always
  // accepted regardless of sort-check.
  if (n == 0) {
    s->goal_lhs = 0;
    s->goal_rhs = 0;
    s->conj_sym_mask = 0;
    s->rel_lvl1_mask = 0;
    s->tgt_ready = 0;
#ifdef THVM_ATPFT_RULES
    s->goal_lhs_ft = NULL;
    s->goal_rhs_ft = NULL;
    s->goal_nf_ft_l = NULL;
    s->goal_nf_ft_r = NULL;
#endif
    atp_goals_release(s);
    return 1;
  }
  // 8.4d: gate on sort-check when a spec is attached -- both
  // sides of every conjunct must be well-sorted AND share a sort.
  if (s->spec != NULL) {
    for (u32 g = 0; g < n; g++) {
      u32 sl = wald_term_sort(s->spec, lhs[g]);
      u32 sr = wald_term_sort(s->spec, rhs[g]);
      if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
        return 0;
      }
    }
  }
  atp_goals_release(s);
  s->goals_lhs    = (Term *)malloc((size_t)n * sizeof(Term));
  s->goals_rhs    = (Term *)malloc((size_t)n * sizeof(Term));
  s->goals_lhs_nf = (Term *)calloc(n, sizeof(Term));
  s->goals_rhs_nf = (Term *)calloc(n, sizeof(Term));
#ifdef THVM_ATPFT_RULES
  s->goals_lhs_ft  = (struct AtpFtCell **)calloc(n, sizeof(void *));
  s->goals_rhs_ft  = (struct AtpFtCell **)calloc(n, sizeof(void *));
  s->goals_nf_ft_l = (struct AtpFtCell **)calloc(n, sizeof(void *));
  s->goals_nf_ft_r = (struct AtpFtCell **)calloc(n, sizeof(void *));
#endif
  if (s->goals_lhs == NULL || s->goals_rhs == NULL ||
      s->goals_lhs_nf == NULL || s->goals_rhs_nf == NULL) {
    atp_goals_release(s);
    return 0;
  }
  for (u32 g = 0; g < n; g++) {
    s->goals_lhs[g] = lhs[g];
#ifdef THVM_ATPFT_RULES
    if (s->goals_lhs_ft != NULL && s->ft_arena_ptr != NULL) {
      s->goals_lhs_ft[g] =
          ft_from_term((AtpFt *)s->ft_arena_ptr, lhs[g], 0);
      s->goals_rhs_ft[g] =
          ft_from_term((AtpFt *)s->ft_arena_ptr, rhs[g], 0);
    }
#endif
    s->goals_rhs[g] = rhs[g];
  }
  s->n_goals = n;
  // ifeq guard-DECIDING registry (see atp_ifeq_guard_register): a
  // conjecture conjunct can carry ifeq-guarded content on the FOF
  // front-end's own encodings even when a pre-encoded '-10' file's
  // final goal is a bare `a != b` (see the tgt-sel goal-cone doc
  // above).  No-op unless use_ifeq_decide is armed.
  for (u32 g = 0; g < n; g++)
    atp_ifeq_guard_register(s, lhs[g], rhs[g]);
  // goal_lhs/goal_rhs alias the first (unjoined) goal -- the storage
  // every single-goal consumer and goal-directed heuristic reads.
  s->goal_lhs = lhs[0];
  s->goal_rhs = rhs[0];
#ifdef THVM_ATPFT_RULES
  // Stage 4: mirror the alias goal pair into the AtpFt arena.  Goals
  // are persistent (live as long as the conjecture); convert with
  // scratch=0.  Set-goal can be called multiple times (test setup,
  // existential mode flip); the previous goal cells become unreachable
  // in the arena but the slab pool never returns memory mid-state,
  // freed wholesale at thvm_atp_free.
  s->goal_lhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, lhs[0], 0);
  s->goal_rhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, rhs[0], 0);
  s->goal_nf_ft_l = NULL;
  s->goal_nf_ft_r = NULL;
#endif
  // Goal changed: the tgt goal-symbol bitset is recollected lazily.
  s->tgt_ready = 0;
  // Recompute the conjecture symbol mask for ATP_CP_WEIGHT_CONJSYM --
  // the union over every conjunct's symbols.
  s->conj_sym_mask = 0;
  for (u32 g = 0; g < n; g++) {
    atp_collect_symbols(lhs[g], &s->conj_sym_mask);
    atp_collect_symbols(rhs[g], &s->conj_sym_mask);
  }
  // Recompute the level-1 relevance mask for back-compat (legacy
  // CONJSYM consumers).  Now also recompute the per-symbol BFS level
  // array sym_level[] for ATP_CP_WEIGHT_RELLEVEL.  Skips dead rules.
  s->rel_lvl1_mask = 0;
  for (u32 i = 0; i < WALD_MAX_SYMBOLS; i++) s->sym_level[i] = ATP_REL_LEVEL_REMOTE;
  for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
    if (s->conj_sym_mask & ((u64)1 << lab)) s->sym_level[lab] = 0u;
  }
  // BFS through the symbol/axiom graph: an axiom links every pair of
  // its symbols.  Mark a symbol at level k+1 if it co-occurs in some
  // axiom with any level-k symbol and is not yet marked at <=k.
  u64 frontier = s->conj_sym_mask;
  for (u8 level = 0; level < (u8)ATP_REL_LEVEL_MAX && frontier; level++) {
    u64 next_frontier = 0;
    for (u32 i = 0; i < s->n_rules; i++) {
      if (s->r_dead[i]) continue;
      u64 rule_syms = 0;
      atp_collect_symbols(s->lhs[i], &rule_syms);
      atp_collect_symbols(s->rhs[i], &rule_syms);
      if (rule_syms & frontier) {
        // Find labels in this axiom not yet marked.
        for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
          if ((rule_syms & ((u64)1 << lab)) &&
              s->sym_level[lab] == ATP_REL_LEVEL_REMOTE) {
            s->sym_level[lab] = level + 1u;
            next_frontier |= ((u64)1 << lab);
            if (level == 0u) s->rel_lvl1_mask |= ((u64)1 << lab);
          }
        }
      }
    }
    frontier = next_frontier;
  }
  // s2a (Vampire sine_to_age): the generality-aware SInE level per symbol.
  // Computed when s2a drives selection OR when the GNN re-rank / graph
  // export needs the goal_closeness node feature -- decoupled from
  // selection (it only fills sym_slvl; atp_cp_sine_level still gates the
  // age bias on s2a_coeff).  Byte-identical default engine otherwise.
  s->goal_levels_ready = 0u;
  if (s->s2a_coeff > 0u || s->record_cp_graphs || s->gnn_rerank_period > 0u) {
    atp_compute_sine_levels(s);
    s->goal_levels_ready = 1u;
  }
  return 1;
}

// 8.4d: attach a WaldSpec for sort-check gating.
fn void thvm_atp_set_spec(AtpState *s, const struct WaldSpec *spec) {
  if (s == NULL) return;
  s->spec = spec;
}

// 8.5c: attach an LpoConfig.  When non-NULL, orient_and_add
// dispatches to thvm_lpo instead of thvm_kbo.
fn void thvm_atp_set_lpo(AtpState *s, const LpoConfig *lpo) {
  if (s == NULL) return;
  s->lpo = lpo;
}

// Attach a RpoConfig.  When non-NULL, atp_compare dispatches to
// thvm_rpo, which generalises LPO via per-symbol status (LEX or MUL).
// RPO takes precedence over LPO when BOTH are set, since RPO subsumes
// LPO via all-LEX status -- callers should set RPO alone for clarity.
fn void thvm_atp_set_rpo(AtpState *s, const RpoConfig *rpo) {
  if (s == NULL) return;
  s->rpo = rpo;
}

// Attach a WpoConfig.  WPO unifies KBO/LPO/RPO via weights + precedence
// + per-symbol status.  Highest-precedence ordering when multiple are
// set: WPO > RPO > LPO > KBO.  Callers should set exactly one.
fn void thvm_atp_set_wpo(AtpState *s, const WpoConfig *wpo) {
  if (s == NULL) return;
  s->wpo = wpo;
}

// Milestone 10: runtime gate for the MNF goal-directed front search.
// No-op effect unless the dylib was compiled with -DATP_MNF -- the
// field is always present, but goal_check only reads it inside its
// `#ifdef ATP_MNF` block.
fn void thvm_atp_set_use_mnf(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_mnf = on ? 1u : 0u;
}

// Runtime gate for the goal-narrow endgame (src/atp/goal_narrow.c).
// Effective only in ATP_ORDERED_REWRITE builds -- the field is always
// present, but goal_check consults it inside that guard.
fn void thvm_atp_set_use_goal_narrow(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_goal_narrow = on ? 1u : 0u;
}

fn void thvm_atp_set_use_ground_join(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_ground_join = on ? 1u : 0u;
}

// Lazy (selection-time) ground-joinability.  Runs the expensive GJ check
// on the SELECTED, R+E-normalized CP instead of on every generated CP at
// push time -- Twee's redundancy discipline.  Default OFF (byte-identical
// engine); armed via the bench THVM_ATP_LAZY_GJ / FindProof
// --lazy-ground-join.  The push-side use_ground_join should stay off under
// this mode so ground-joinability is paid exactly once, at pop.
fn void thvm_atp_set_use_lazy_ground_join(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_lazy_ground_join = on ? 1u : 0u;
}

// Bachmair-Dershowitz connectedness CP deletion (Twee section 6.2).
// Default OFF -> the engine is byte-identical; the WL surface flips it
// for Method -> {... "Connectedness" -> True} and the Waldmeister preset.
fn void thvm_atp_set_use_connectedness(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_connectedness = on ? 1u : 0u;
}

// Waldmeister-faithful RHS interreduction (Interreduktion.c
// RMRechtsInterred).  See the AtpState.use_rhs_interreduce comment.
fn void thvm_atp_set_use_rhs_interreduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_rhs_interreduce = on ? 1u : 0u;
}

// Unfailing-completion both-faces superposition.  See the
// AtpState.use_unfailing_cp comment.
fn void thvm_atp_set_use_unfailing_cp(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_unfailing_cp = on ? 1u : 0u;
}

fn void thvm_atp_set_use_flatterm(AtpState *s, u8 on) {
  if (s == NULL) return;
#ifdef ATP_RULE_INDEX
  s->use_flatterm = on ? 1u : 0u;
#else
  (void)on;   // no flat machinery without the rule index
#endif
}

fn void thvm_atp_set_use_cp_index(AtpState *s, u8 on) {
  if (s == NULL) return;
#ifdef ATP_RULE_INDEX
  s->use_cp_index = on ? 1u : 0u;
#else
  (void)on;   // no index machinery without the rule index
#endif
}

fn void thvm_atp_set_selection_ratio(AtpState *s, u32 modulo) {
  if (s == NULL) return;
  s->fifo_modulo = modulo;   // 0 -> default (11) at selection time
}

// Vampire-style random CP-selection mode.  `modulo == 0` disables (engine
// byte-identical); `modulo > 0` makes every nth selection pick a
// uniformly-random queued CP via the deterministic xorshift64 stream
// seeded by `thvm_atp_set_random_seed`.  Pairs with the heap-min default
// the same way `fifo_modulo` does.
fn void thvm_atp_set_random_modulo(AtpState *s, u32 modulo) {
  if (s == NULL) return;
  s->random_modulo = modulo;
}
fn void thvm_atp_set_random_seed(AtpState *s, u64 seed) {
  if (s == NULL) return;
  // xorshift64 forbids state 0; coerce to a nonzero default if the
  // caller passes 0 (which means "default seed" rather than "no
  // randomness"; randomness itself is gated on random_modulo > 0).
  s->rng_state = seed ? seed : 0x9E3779B97F4A7C15ull;
}

// Select the CP-priority weight mode (an `AtpCpWeightMode` value).
// Out-of-range values clamp to ATP_CP_WEIGHT_ADD (0) so a garbage
// mode falls back to the bare symbol-count heuristic.
fn void thvm_atp_set_cp_weight_mode(AtpState *s, u32 mode) {
  if (s == NULL) return;
  s->cp_weight_mode = (mode < ATP_CP_WEIGHT_LAST)
                    ? (u8)mode
                    : (u8)ATP_CP_WEIGHT_ADD;
}

fn void thvm_atp_set_max_cp_weight(AtpState *s, u32 w) {
  if (s != NULL) s->max_cp_weight = w;
}

fn void thvm_atp_set_max_cp_side(AtpState *s, u32 w) {
  if (s != NULL) s->max_cp_side = w;
}

fn void thvm_atp_set_max_cp_queue(AtpState *s, u32 n) {
  if (s != NULL) s->max_cp_queue = n;
}

// Enable the automatic, completeness-preserving growing CP-weight
// bound.  `base` seeds the bound; slope (default 2) scales it by the
// deepest current rule LHS so the bound tracks how complex the rule
// set has become.  base==0 disables (the historical unbounded engine).
fn void thvm_atp_set_auto_max_cp_weight(AtpState *s, u32 base) {
  if (s == NULL) return;
  s->auto_max_cp_weight_base  = base;
  s->auto_max_cp_weight_slope = 2u;
  s->auto_max_cp_weight_cur   = base;   // grown lazily as rules deepen
}

fn void thvm_atp_set_goal_interleave(AtpState *s, u32 ratio) {
  if (s != NULL) s->use_goal_interleave = ratio;
}

// s2a (Vampire sine_to_age): arm the goal-distance age postponement with
// coefficient `coeff` (0 disables, engine byte-identical).  Recomputes the
// per-symbol SInE levels now when a goal is already attached; set_goals
// recomputes them too when the goal is (re)set afterwards.
fn void thvm_atp_set_s2a(AtpState *s, u32 coeff) {
  if (s == NULL) return;
  s->s2a_coeff = coeff;
  if (coeff > 0u && s->goal_lhs != 0) atp_compute_sine_levels(s);
}

// 8.5c: order-aware compare.  Picks LPO (if attached) or KBO,
// returning a unified KboCmp-shaped result.  The two enums share
// numeric values (EQ=0, GT=1, LT=-1, UN=2), so the cast is safe.
//
// Stage 8: a `THVM_ATP_LPO_ORIENT_CACHE` wrapper sits on top of this
// body (see below) and memoises the verdict by structural-hash pair.
// WM Col3 combinator ordering (Ordnungen.c:101-117 Col3Test /
// Col3Vergleich + TermOperationen.c:1284-1315 IstKombinatorTupel).
// s > t iff
//   (a) s is a combinator redex `C x1 .. xn` on the apply spine:
//       peeling `apply` off s's LEFT spine reaches a constant head C,
//       and the spine arguments x1..xn are pairwise-DISTINCT
//       variables; and t is built ONLY from the apply operator plus
//       variables occurring in s -- NO constants at all
//       (SO_SymbIstFkt covers arity-0 symbols, so WM's t-walk rejects
//       any constant != apply: the Y fixpoint contractum x(Yx)
//       carries Y and stays UNORIENTABLE, kept as E); or
//   (b) s is a bare constant and t is ground (WM guards TRUE/FALSE
//       under existential goals; the equational bench has none).
// Not a simplification order: combinator reduction duplicates
// variables.  Completion under it relies on the same step caps WM's
// KombS strategy does.
static int atp_col3_tuple(Term s, Term t, u32 apply) {
  Term args[64];
  u32 n = 0;
  if (!(term_tag(s) == TAG_CTR && term_ext(s) == apply &&
        term_ctr_n(s) == 2u)) {
    return 0;
  }
  Term cur = s;
  while (term_tag(cur) == TAG_CTR && term_ext(cur) == apply &&
         term_ctr_n(cur) == 2u) {
    if (n >= 64u) return 0;
    args[n++] = term_ctr_at(cur, 1);
    cur = term_ctr_at(cur, 0);
  }
  if (!(term_tag(cur) == TAG_CTR && term_ctr_n(cur) == 0u)) return 0;
  for (u32 i = 0; i < n; i++) {
    if (term_tag(args[i]) != TAG_FVR) return 0;
    for (u32 j = i + 1u; j < n; j++) {
      if (term_ext(args[i]) == term_ext(args[j])) return 0;
    }
  }
  // t's preorder: every function symbol (constants included) must be
  // the apply operator; variables must be among the spine args.
  Term stk[256];
  u32 sp = 0u;
  stk[sp++] = t;
  while (sp > 0u) {
    Term c = stk[--sp];
    if (term_tag(c) == TAG_FVR) {
      u32 id = term_ext(c);
      u32 ok = 0u;
      for (u32 i = 0; i < n; i++) {
        if (term_ext(args[i]) == id) { ok = 1u; break; }
      }
      if (!ok) return 0;
      continue;
    }
    if (term_tag(c) != TAG_CTR) return 0;
    if (term_ext(c) != apply || term_ctr_n(c) == 0u) return 0;
    u32 cn = term_ctr_n(c);
    if (sp + cn > 256u) return 0;
    for (u32 i = 0; i < cn; i++) stk[sp++] = term_ctr_at(c, i);
  }
  return 1;
}

static int atp_term_is_ground(Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  int   ground = 1;
  for (;;) {
    switch (term_tag(t)) {
      case TAG_FVR:
        ground = 0;
        break;
      case TAG_CTR: {
        u32 n = term_ctr_n(t);
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
        break;
      }
      default:
        break;
    }
    if (!ground || nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return ground;
}

static int atp_col3_test(Term s, Term t, u32 apply) {
  if (atp_col3_tuple(s, t, apply)) return 1;
  if (term_tag(s) == TAG_CTR && term_ctr_n(s) == 0u &&
      atp_term_is_ground(t)) {
    return 1;
  }
  return 0;
}

static KboCmp atp_col3(Term lhs, Term rhs, u32 apply) {
  if (kbo_eq(lhs, rhs)) return KBO_EQ;
  if (atp_col3_test(lhs, rhs, apply)) return KBO_GT;
  if (atp_col3_test(rhs, lhs, apply)) return KBO_LT;
  return KBO_UN;
}

// Classification probe for the -auto mirror (the bench's
// Kombinatorlogik detection): is (a, b) a combinator-definition
// equation in either direction under `apply`?
fn u8 thvm_atp_col3_pair(Term a, Term b, u32 apply) {
  return (atp_col3_tuple(a, b, apply) || atp_col3_tuple(b, a, apply))
             ? 1u : 0u;
}

// With the flag off, the macro alias below collapses back to this
// function and the default build is byte-identical.
static KboCmp atp_compare_uncached(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
  if (s->col3_apply != 0u) return atp_col3(lhs, rhs, s->col3_apply);
#ifdef THVM_ATP_AC
  // AC-aware ordering: when an AC mask is registered, route through
  // atp_kbo_ac / atp_lpo_ac.  They canonicalize both terms modulo
  // AC and apply the syntactic ordering to the canonical forms, so
  // the verdict is AC-invariant by construction.  Without this,
  // AC-permuted siblings flip orientation on every superposition
  // step and the trajectory blows up.
  u64 ac_mask = thvm_atp_get_ac_mask();
  if (ac_mask != 0ull) {
    AtpAcInfo ac = { .ac_mask = ac_mask };
    // WPO/RPO under AC: canonicalize both terms modulo AC, then apply
    // the syntactic ordering.  WPO > RPO > LPO precedence among set
    // configs; callers should set exactly one.
    if (s->wpo != NULL) {
      AtpAcInfo ac2 = ac;
      Term cl = atp_ac_canon(lhs, &ac2);
      Term cr = atp_ac_canon(rhs, &ac2);
      return (KboCmp)thvm_wpo(cl, cr, s->wpo);
    }
    if (s->rpo != NULL) {
      AtpAcInfo ac2 = ac;
      Term cl = atp_ac_canon(lhs, &ac2);
      Term cr = atp_ac_canon(rhs, &ac2);
      return (KboCmp)thvm_rpo(cl, cr, s->rpo);
    }
    if (s->lpo != NULL) {
      return (KboCmp)atp_lpo_ac(lhs, rhs, s->lpo, &ac);
    }
    return atp_kbo_ac(lhs, rhs, s->kbo, &ac);
  }
#endif
  if (s->wpo != NULL) {
    return (KboCmp)thvm_wpo(lhs, rhs, s->wpo);
  }
  if (s->rpo != NULL) {
    return (KboCmp)thvm_rpo(lhs, rhs, s->rpo);
  }
  if (s->lpo != NULL) {
    return (KboCmp)thvm_lpo(lhs, rhs, s->lpo);
  }
#ifdef ATP_RULE_INDEX
  // Optional flatterm KBO: thvm_kbo_flat runs the identical Loechner
  // decision over a cache-dense pre-order node array.  Measured net-
  // NEGATIVE on this engine: thvm's CTR cells already store their
  // children contiguously (term_ctr_at = heap_read(base + i)), so the
  // IC traversal is itself cache-friendly and the flatten-both-operands
  // pass is pure overhead.  The win that DID land is in thvm_kbo's
  // traversal (one child-base read per node instead of a redundant
  // term_ctr_n per child access), so the IC comparator is the default
  // fast path.  thvm_kbo_flat stays opt-in (THVM_ATP_KBO_FLAT=1) behind
  // use_flatterm for A/B and the differential self-check; unset / "0" =
  // the optimized IC KBO.  Cached once; -1 = unread.
  static int kbo_flat_gate = -1;
  if (kbo_flat_gate < 0) {
    const char *e = getenv("THVM_ATP_KBO_FLAT");
    kbo_flat_gate = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  if (s->use_flatterm && kbo_flat_gate) {
    return thvm_kbo_flat(lhs, rhs, s->kbo);
  }
#endif
  return thvm_kbo(lhs, rhs, s->kbo);
}

// Stage 8 wrapper: structural-hash-keyed verdict cache.  See
// src/atp/lpo_cache.c for the storage + invalidation.  With the flag
// off this collapses to a direct call -- default build byte-identical.
#ifdef THVM_ATP_LPO_ORIENT_CACHE
static KboCmp atp_compare(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
  u64 lh = atp_term_struct_hash(lhs);
  u64 rh = atp_term_struct_hash(rhs);
  KboCmp cached;
  int hit = atp_lpo_orient_cache_get(lh, rh, &cached);
  if (hit && !atp_lpo_orient_verify_gate()) return cached;
  KboCmp fresh = atp_compare_uncached(s, lhs, rhs);
  if (hit && cached != fresh) {
    char lb[2048], rb[2048];
    atp_pretty_term(lhs, lb, sizeof lb);
    atp_pretty_term(rhs, rb, sizeof rb);
    fprintf(stderr, "ATP_LPO_ORIENT_CACHE MISMATCH cached=%d fresh=%d\n"
                    " lh=0x%016llx rh=0x%016llx\n lhs=%s\n rhs=%s\n",
            (int)cached, (int)fresh,
            (unsigned long long)lh, (unsigned long long)rh, lb, rb);
    abort();
  }
  if (!hit) atp_lpo_orient_cache_put(lh, rh, fresh);
  return fresh;
}
#else
static inline KboCmp atp_compare(AtpState *s, Term lhs, Term rhs) {
  return atp_compare_uncached(s, lhs, rhs);
}
#endif

