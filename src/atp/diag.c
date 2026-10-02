// === DIAGNOSTIC (Step 1) ============================================
// Measure the interreduction deficit and term-size growth of R.  For
// each rule i, normalize lhs[i] / rhs[i] against R \ {i} (linear
// oracle, full re-scan).  A side that changes is reducible by another
// rule -- the rule is NOT interreduced.  Reports counts + max/total
// symbol footprint.  Read-only; no engine state mutated.  Used by the
// bench to confirm whether R is genuinely under-interreduced.
fn void thvm_atp_interreduce_deficit(AtpState *s,
                                     u32 *n_lhs_reducible,
                                     u32 *n_rhs_reducible,
                                     u32 *n_any_reducible,
                                     u32 *max_side_symbols,
                                     u64 *total_symbols) {
  u32 nl = 0, nr = 0, na = 0, maxside = 0;
  u32 n_red_unorient = 0;
  u64 total = 0;
  if (s == NULL || s->n_rules == 0) goto done;
  u32 n = s->n_rules;
  Term *tl = (Term *)malloc((size_t)n * sizeof(Term));
  Term *tr = (Term *)malloc((size_t)n * sizeof(Term));
  if (tl == NULL || tr == NULL) { free(tl); free(tr); goto done; }
  for (u32 i = 0; i < n; i++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    u32 ls = atp_rule_symbol_count(s, i, 0u);
    u32 rs = atp_rule_symbol_count(s, i, 1u);
#else
    u32 ls = atp_symbol_count(s->lhs[i]);
    u32 rs = atp_symbol_count(s->rhs[i]);
#endif
    if (ls > maxside) maxside = ls;
    if (rs > maxside) maxside = rs;
    total += (u64)ls + (u64)rs;
    // Build R \ {i}.
    u32 m = 0;
    for (u32 j = 0; j < n; j++) {
      if (j == i) continue;
      tl[m] = s->lhs[j];
      tr[m] = s->rhs[j];
      m++;
    }
    Term ln = thvm_rewrite_normalize(s->lhs[i], tl, tr, m, 256);
    Term rn = thvm_rewrite_normalize(s->rhs[i], tl, tr, m, 256);
    int lred = !kbo_eq(ln, s->lhs[i]);
    int rred = !kbo_eq(rn, s->rhs[i]);
    if (lred) nl++;
    if (rred) nr++;
    if (lred || rred) {
      na++;
      if (!s->r_orient[i]) n_red_unorient++;
      if (getenv("THVM_ATP_DIAG_DUMP") != NULL) {
        char la[1024], ra[1024], na_[1024];
        atp_pretty_term(s->lhs[i], la, sizeof la);
        atp_pretty_term(s->rhs[i], ra, sizeof ra);
        atp_pretty_term(lred ? ln : rn, na_, sizeof na_);
        // Which single rule reduces side i at the top?  (single-rule
        // collapse vs multi-rule chain.)
        int single = -1;
        Term side = lred ? s->lhs[i] : s->rhs[i];
        for (u32 j = 0; j < n; j++) {
          if (j == i) continue;
          Term one = thvm_rewrite_normalize(side, &s->lhs[j], &s->rhs[j], 1u, 4u);
          if (!kbo_eq(one, side)) { single = (int)j; break; }
        }
        fprintf(stderr, "    REDUCIBLE rule %u%s: %s -> %s   [%s side -> %s] "
                "(single-rule reducer: %d, orient_unred=%u)\n",
                i, s->r_orient[i] ? "" : "(un)", la, ra,
                lred ? "lhs" : "rhs", na_, single, n_red_unorient);
      }
    }
  }
  free(tl);
  free(tr);
done:
  if (n_lhs_reducible)  *n_lhs_reducible  = nl;
  if (n_rhs_reducible)  *n_rhs_reducible  = nr;
  if (n_any_reducible)  *n_any_reducible  = na;
  if (max_side_symbols) *max_side_symbols = maxside;
  if (total_symbols)    *total_symbols    = total;
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

#define ATP_CP_BATCH 4096

// Stage 7.1: trivial-joinability check AND critical-pair reduction.
// Normalize both sides of a candidate CP under the current rule set R
// and write the reduced pair back through `*lhs`/`*rhs`: standard
// completion queues the NORMALIZED CP, not the raw overlap.  The
// un-normalized overlap of two deep rules blows past thousands of
// nodes -- dragging every later KBO compare / match / index descent
// and overrunning the retrieval flatten cap into a full O(n) scan.
// Returns 1 iff the two normal forms collapse to one term: the CP is
// joinable-by-R, adds no new consequence, and the caller discards it.
//
// This is the Waldmeister `Grundzusammenfuehrung` ("ground-merging")
// criterion at its weakest, equivalent to Twee's "joinable-by-current-
// R" pruning.  Stronger variants (ground-joinability over a sample
// of substitutions, AC-aware joinability) are deferred to 7.2+.
//
// Cost: two `thvm_rewrite_normalize` calls per CP candidate.  Worth
// it when the saturation produces many joinable CPs (group axioms
// generate ~hundreds of trivially-joinable overlaps per round).
// Joinability cache: maps (lhs_hash ^ rhs_hash, R-epoch, n_rules) to
// the trivially-joinable verdict.  Stores only the boolean, not Term
// cells -- safe across heap_reset (which doesn't bump unf_memo_epoch
// directly anymore since the norm-result-cache port was reverted).
// On hit when the verdict is 0 (NOT joinable), we still have to
// normalize for the caller's *lhs/*rhs write-back so the cache
// short-circuit only helps when joined.  When the verdict is 1
// (joined), the caller drops the CP and doesn't need the NFs, so
// we skip the normalize entirely on hit -- the structural lever.
#define ATP_JOIN_CACHE_BITS 16
#define ATP_JOIN_CACHE_SIZE (1u << ATP_JOIN_CACHE_BITS)
#define ATP_JOIN_CACHE_MASK (ATP_JOIN_CACHE_SIZE - 1u)
typedef struct { u64 key; u32 epoch; u32 n_rules; u8 joined; } AtpJoinCacheEnt;
static AtpJoinCacheEnt g_atp_join_cache[ATP_JOIN_CACHE_SIZE];
u64 g_atp_join_cache_hits   = 0;
u64 g_atp_join_cache_misses = 0;

// Ensure the FT mirror is populated for every live rule before any
// caller of atp_rewrite_normalize_ft / atp_cp_trivially_joinable_ft.
// Production saturations (atp_push_rule) populate in lockstep with
// s->lhs[] writes, so this loop is empty after the first probe; test
// harnesses that write s->lhs[] directly get the mirror built on
// demand here so the FT normalize sees the whole rule set instead of
// silently skipping NULL-mirror slots.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static inline void atp_ft_mirror_ensure(AtpState *s) {
  if (s->ft_mirror_full && s->n_rules <= s->ft_mirror_probed_n_rules) return;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  u32 start = s->ft_mirror_full ? s->ft_mirror_probed_n_rules : 0u;
  for (u32 i = start; i < s->n_rules; i++) {
    if (s->lhs[i] != 0 && s->lhs_ft[i] == NULL) {
      s->lhs_ft[i] = ft_from_term(a, s->lhs[i], 0);
      s->rhs_ft[i] = ft_from_term(a, s->rhs[i], 0);
    }
  }
  s->ft_mirror_full = 1u;
  s->ft_mirror_probed_n_rules = s->n_rules;
}

// Term-side simpler-join wrapper (pop path, record/legacy arms):
// convert the two NF Terms, run the FT pass (which clones internally,
// so fl/fr stay intact), free the conversions.  See
// atp_cp_sj_joinable_ft (ft_norm.c) for the discipline.
static u8 atp_cp_sj_joinable_term(AtpState *s, Term l, Term r) {
  if (l == 0 || r == 0) return 0u;
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = ft_from_term(a, l, 0);
  AtpFtCell *fr = ft_from_term(a, r, 0);
  u8 joined = atp_cp_sj_joinable_ft(s, fl, fr);
  ft_free_span(a, fl, fl->end);
  ft_free_span(a, fr, fr->end);
  return joined;
}
#endif

// FT-only trivial-joinable check.  Term cells in (*lhs / *rhs), the work
// happens entirely in FT (ft_from_term / normalize / ft_eq), and the NFs
// are decoded back to Term only when the NOT-joined path's downstream
// filters (perm_sub, queue_sub, AC-eq) need them.  The Term-side
// `atp_rewrite_normalize` legacy path used to live behind a default-off
// VERIFY toggle but only ever lagged the FT verdict (and aborted spuriously
// on AndAssoc post-1d0a8035, where Term-side stops one rewrite step short
// of the FT-side NF -- see project_ftnorm_andassoc_verify_break); deleted.
//
// This is the generation-time CP treatment -- WM's `KPBehandelt`
// (INF/KPVerwaltung.c:439-467).  WM's default proof configuration runs
// it with NF_Normalform2(doR=TRUE, doE=FALSE) (`-kg` default "r",
// RUN/Parameter.c:397): oriented rules only, the unorientable-equation
// tree (and its grounded free-variable instances) never rewrites here.
// Hence atp_rules_only_normalize_ft, not atp_rewrite_normalize_ft.  The
// deferred full doR+doE normalize and join verdict happen at selection
// (thvm_atp_step's pop normalize = WM `KPV_Select`, `-ks` "r:e:s:p").
// Current batch's NEW fact slot (the fact being activated); the push
// path's self-rederivation guard compares against THIS slot only --
// WM blocks only the just-activated fact as a KPBehandeln reducer.
static u32 g_atp_batch_new_slot = 0xffffffffu;

static u8 atp_cp_trivially_joinable(AtpState *s, Term *lhs, Term *rhs) {
  // Detail-tier timing (THVM_ATP_PROFILE=2): live only for push-loop
  // calls (g_atp_pushn_active).  _dt walks forward through the call;
  // each PUSHN_TICK charges the elapsed slice to one component.
  int _dl = (g_atp_phase_detail && g_atp_pushn_active);
  u64 _dt = _dl ? atp_now_us() : 0;
#define PUSHN_TICK(counter) \
  do { if (_dl) { u64 _n = atp_now_us(); (counter) += _n - _dt; _dt = _n; } } while (0)
  if (_dl) g_atp_pushn_calls++;
  static int dbg_join_cache = -1;
  if (dbg_join_cache < 0) dbg_join_cache = atp_env_on("THVM_ATP_JOIN_CACHE");
  u64 join_key = 0;
  u8  join_cache_eligible = 0;
  // Simpler-join mode 1 verdicts read the CP's PEAK (checkOrder/step3),
  // so the content-keyed cache would alias same-pair CPs from different
  // overlaps -- ineligible.
  if (dbg_join_cache && s->use_simpler_join != 1u) {
    u64 lh = atp_term_struct_hash(*lhs);
    u64 rh = atp_term_struct_hash(*rhs);
    // Symmetric: joinable(l, r) == joinable(r, l), so the mix is symmetric.
    join_key = (lh + rh) ^ (lh * rh + 0x9e3779b97f4a7c15ull);
    AtpJoinCacheEnt *e = &g_atp_join_cache[(u32)join_key & ATP_JOIN_CACHE_MASK];
    if (e->key == join_key && e->epoch == g_atp_unf_memo_epoch
        && e->n_rules == s->n_rules) {
      // Cached verdict is sound under (epoch + n_rules) gating: any R change
      // bumps one of them.  Short-circuit both branches, not just joined=1.
      g_atp_join_cache_hits++;
      PUSHN_TICK(g_atp_pushn_us_hash);
      return e->joined;
    }
    join_cache_eligible = 1u;
    PUSHN_TICK(g_atp_pushn_us_hash);
  }
  u8 joined = 0u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  atp_ft_mirror_ensure(s);
  PUSHN_TICK(g_atp_pushn_us_mirror);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = ft_from_term(a, *lhs, 0);
  AtpFtCell *fr = ft_from_term(a, *rhs, 0);
  PUSHN_TICK(g_atp_pushn_us_from);
#ifdef THVM_ATPFT_CPQ
  joined = atp_cp_trivially_joinable_ft(s, &fl, &fr);
#else
  fl = atp_rules_only_normalize_ft(s, fl, 64u);
  fr = atp_rules_only_normalize_ft(s, fr, 64u);
  PUSHN_TICK(g_atp_pushn_us_norm);
  joined = (u8)ft_eq(fl, fr);
  PUSHN_TICK(g_atp_pushn_us_eq);
  // Simpler-join second chance (see atp_cp_trivially_joinable_ft);
  // mode 2 (pop-only) skips the generation-time arm.
  if (!joined && s->use_simpler_join == 1u) {
    joined = atp_cp_sj_joinable_ft(s, fl, fr);
    if (joined) s->n_cps_dropped_simpler_join++;
  }
#endif
  // Decode NFs back to Term only on the NOT-joined branch -- downstream
  // filters (perm_sub, queue_sub, AC-eq) need them; the joined branch
  // drops the CP and never reads *lhs/*rhs again.
  if (!joined) {
    Term l = ft_to_term(fl);
    Term r = ft_to_term(fr);
    *lhs = l;
    *rhs = r;
    PUSHN_TICK(g_atp_pushn_us_to);
#ifdef THVM_ATP_AC
    // AC-equality redundancy: when an AC bitmask is registered, treat
    // AC-equal NFs as joinable.  Sound iff the AC axioms for every masked
    // label are present in R (normalize would have closed any AC-equal
    // pair via those axioms eventually, so the CP is redundant).
    if (thvm_atp_get_ac_mask() != 0ull) {
      AtpAcInfo ac = { .ac_mask = thvm_atp_get_ac_mask() };
      if (atp_ac_eq(l, r, &ac)) joined = 1u;
    }
    PUSHN_TICK(g_atp_pushn_us_ac);
#endif
  }
  if (_dl && joined) g_atp_pushn_joined++;
#else
  // No FT compiled in: caller must use a Term-only joinable check.  This
  // function isn't usable, so the cache write is skipped too.
  (void)lhs; (void)rhs;
#endif
#undef PUSHN_TICK
  if (join_cache_eligible) {
    g_atp_join_cache_misses++;
    AtpJoinCacheEnt *e = &g_atp_join_cache[(u32)join_key & ATP_JOIN_CACHE_MASK];
    e->key     = join_key;
    e->epoch   = g_atp_unf_memo_epoch;
    e->n_rules = s->n_rules;
    e->joined  = joined;
  }
  return joined;
}

// Permutation-subsumption (port of WM `GZ_ACVerzichtbar` in
// Grundzusammenfuehrung.c:137).  Returns 1 iff (lhs, rhs) are
// AC-equivalent: they have the same top symbol AND the same multiset
// of immediate children, where two children are "the same" if they
// are themselves AC-equivalent or structurally equal (kbo_eq).
//
// Catches `nand(x, y) = nand(y, x)` and its nested variants, e.g.:
//   nand(nand(x,y), z)     = nand(nand(y,x), z)
//   nand(nand(x,y), nand(x,y)) = nand(nand(y,x), nand(x,y))
// WM drops such CPs at push + orient time when the top symbol is not
// AC-marked, preventing the cascade of commutativity-derived rules
// that dominates the AndAssoc faithful-port trajectory.
//
// Arity-2 only (Sheffer signature).  Higher arities would need a
// multiset matching with backtracking; defer.
// Per-symbol AC mask: bit i = "label i's CTRs are subject to the
// permutation-subsumption filter".  Set globally by the bench harness
// (env THVM_ATP_PERM_SUB_MASK) and queried inline below.  Mask of 0
// means "no per-symbol restriction" -- legacy behaviour where every
// AC-equal CP is dropped.  Mask != 0 means "only drop when the top
// symbol is in the mask" -- lets the WL paclet enable perm_sub for
// problems like AndAssoc (set bit 1 = nand) without breaking mccune
// (whose `and` symbol is intentionally outside the mask so its
// commutativity-shaped goal CP survives).  The global itself is
// defined up with the early ATP state (the enqueue / pop gates read
// it before this point); only the setter lives here.
fn void thvm_atp_set_perm_subsume_mask(u64 mask) {
  g_atp_perm_subsume_mask = mask;
}

// WM GZ_ACVerzichtbar's `!IstAssoz && !IstKommut && !IstErwKommut`
// exclusion lives below (atp_gj_perm_valuable: the A/C/C' shapes the
// ground-join fact test also protects).  Forward-declared so the
// AC-dispensability path can keep the generating axioms.
static u8 atp_gj_perm_valuable(Term l, Term r);

// Port of WM GZ_ACVerzichtbar (INF/Grundzusammenfuehrung.c:137), the
// AC-permutation-redundancy filter `dokgP`/`Permsub` runs at CP
// formation (KPVerwaltung.c:447) and at selection (:663), default-ON
// in the unconfigured Orkus run.  A CP whose two sides are AC-equal
// (equal as flattened+sorted multisets under the AC operator) says
// nothing the AC equations have not already said, so WM discards it.
// The two generating shapes -- commutativity and associativity (and
// extended commutativity) -- are kept (they ARE the AC theory).
//
//   if TO_ACGleich(l,r):                  // atp_ac_eq, flatten+sort
//     if !IstACSymbol(top):               // AC-equal but non-AC top
//       return TRUE                       // (e.g. not(or(..))=not(or(..)))
//     else: return !IstAssoz && !IstKommut && !IstErwKommut
//   else: return FALSE
//
// thvm's binary-only swap recursion (used when no AC mask is set, the
// legacy env-driven THVM_ATP_PERM_SUB_MASK path) cannot see that
// `or(a,or(b,c))` and `or(b,or(c,a))` are AC-equal: it never flattens
// the associative spine.  That gap is exactly the Huntington/Boolean/
// Ring/Meredith over-formation -- thvm kept thousands of 4-ary
// `or(a,or(b,or(c,d)))#<perm>` CPs WM perm-subsumes.
//
// Built only under THVM_ATP_AC (atp_ac_eq + the AcInfo live in ac.c).
// The default `bin/test_atp` build omits the flag; there the perm-
// subsume mask can never be set (the AC detector is unavailable), so
// the dispatch below is dead and the function need not exist.
#ifdef THVM_ATP_AC
static u8 atp_cp_ac_dispensable(Term lhs, Term rhs, const AtpAcInfo *ac) {
  if (kbo_eq(lhs, rhs)) return 0;               // t = t handled upstream
  // TO_ACGleich: AC-equal under the perm-subsume AC operators
  // (atp_ac_eq flattens + hash-sorts + structurally compares).  This
  // `ac` is the perm-subsume mask, kept SEPARATE from the engine-wide
  // g_atp_ac_info that gates AC-unification/AC-matching: WM does plain
  // KB completion (no AC-unification) and only filters CPs with this
  // test, so activating thvm's AC-saturation here would diverge.
  if (!atp_ac_eq(lhs, rhs, ac)) return 0;
  // !SO_IstACSymbol(top): AC-equal but the top symbol is not itself
  // an AC operator (the inner AC structure permuted under a non-AC
  // head, e.g. not(or(a,b)) = not(or(b,a))).  Drop unconditionally.
  if (!atp_ac_is_ac_top(ac, lhs)) return 1;
  // Top is an AC operator: keep the generating assoc/comm/ext-comm
  // axioms, drop every other AC-equal pair.
  return !atp_gj_perm_valuable(lhs, rhs);
}
#endif

// The structural gates of one perm-subsumption call: two binary CTRs
// over the same label, the label inside the perm-subsume mask when one
// is set.
static u8 atp_perm_binary_pair(Term lhs, Term rhs) {
  if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
  if (term_ext(lhs) != term_ext(rhs)) return 0;
  // Per-symbol mask gate: when set, only fire perm_sub for symbols in
  // the mask.  Top-symbol bit outside the mask -> keep (return 0).
  if (g_atp_perm_subsume_mask != 0ull) {
    u32 lab = term_ext(lhs);
    if (lab >= 64u || ((g_atp_perm_subsume_mask >> lab) & 1ull) == 0ull) {
      return 0;
    }
  }
  u32 nl = term_ctr_n(lhs), nr = term_ctr_n(rhs);
  return (u8)(nl == nr && nl == 2u);
}

static u8 atp_cp_perm_subsumed(Term lhs, Term rhs) {
  // Two binary nodes are AC-same iff their children pair up -- in order
  // (l0~r0 and l1~r1) or swapped (l0~r1 and l1~r0) -- where a child pair
  // is AC-same iff structurally equal OR itself a permutation; this
  // catches deep commutativity.  Each open pair is a frame stepping
  // through those four child tests (`st` = the next, `res` = results by
  // bit), evaluated in the recursion's order: both in-order tests, and
  // the two swapped ones only when that fails.  A test kbo_eq does not
  // settle opens the nested pair (cl, cr).
  typedef struct { Term l; Term r; u8 st; u8 res; } PermFrame;
  enum { PERM_INL = 64 };
  PermFrame  fr_inl[PERM_INL];
  PermFrame *fr  = fr_inl;
  u32        nfr = 0u;
  u32        cfr = PERM_INL;
  Term       cl  = lhs, cr = rhs;
  u8         v   = 0u;
  for (;;) {
    u8 open = 0u;
    if (cl == cr) {
      v = 0u;                                    // identical handled elsewhere
    }
#ifdef THVM_ATP_AC
    // WM-faithful path: when the AC operators have been detected and
    // registered in the perm-subsume mask (thvm_atp_set_perm_subsume_mask
    // from thvm_atp_auto_ac), use the full flattened AC-dispensability
    // test (GZ_ACVerzichtbar).  This catches the n-ary associativity
    // permutations the binary swap below misses.  The mask is a LOCAL
    // AtpAcInfo, NOT the engine-global g_atp_ac_info -- so the engine
    // stays in standard (non-AC) completion exactly like Waldmeister.
    else if (g_atp_perm_subsume_mask != 0ull) {
      AtpAcInfo ac = { .ac_mask = g_atp_perm_subsume_mask };
      v = atp_cp_ac_dispensable(cl, cr, &ac);
    }
#endif
    else if (!atp_perm_binary_pair(cl, cr)) {
      v = 0u;
    } else {
      if (nfr == cfr) {
        fr = (PermFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].l   = cl;
      fr[nfr].r   = cr;
      fr[nfr].st  = 0u;
      fr[nfr].res = 0u;
      nfr++;
      open = 1u;
    }
    u8 call = 0u;
    while (!call) {
      if (!open) {
        if (nfr == 0u) {
          term_stack_release(fr, fr_inl);
          return v;
        }
        PermFrame *f = &fr[nfr - 1u];
        if (v) {
          f->res |= (u8)(1u << f->st);
        }
        f->st++;
      }
      open = 0u;
      PermFrame *f = &fr[nfr - 1u];
      for (;;) {
        if (f->st == 2u && (f->res & 3u) == 3u) {
          // Identical (as multisets, same order):  caller (trivial-join /
          // duplicate-rule guard) handles this; we don't drop it because
          // the equation is `t = t` which should have been spotted upstream.
          // But pure structural equality (l0 == r0 && l1 == r1 with no
          // perm) is already caught earlier; this branch fires only when
          // the children are AC-equal under deeper permutation.  Treat it
          // as perm-subsumed: the equation says nothing new beyond what
          // its (already-derived) children-AC-equality said.
          v = 1u;
          break;
        }
        if (f->st == 4u) {
          v = (u8)((f->res & 12u) == 12u);
          break;
        }
        Term x = term_ctr_at(f->l, (f->st == 0u || f->st == 2u) ? 0u : 1u);
        Term y = term_ctr_at(f->r, (f->st == 0u || f->st == 3u) ? 0u : 1u);
        if (kbo_eq(x, y)) {
          f->res |= (u8)(1u << f->st);
          f->st++;
          continue;
        }
        cl   = x;
        cr   = y;
        call = 1u;
        break;
      }
      if (!call) {
        nfr--;                                   // decided: v goes to the parent
      }
    }
  }
}

// Stage 7.2b: source-rule-disjoint connectedness check.  Returns 1
// if (lhs, rhs) is joinable under R \ {rule_a, rule_b} -- the two
// rules that birthed this CP via overlap unification.  Bachmair-
// Dershowitz-Plaisted-style redundancy: if the join can be done
// without using either parent rule, the parent rules' interaction
// was redundant.
//
// Per the domination lemma in `docs/plans/connectedness_design.md`,
// this is strictly weaker pruning than 7.1's full-R joinability
// (since reducing the rule set cannot uncover joins that the full
// set misses).  We compute it for measurement: the resulting
// counter `n_cps_dropped_connected` is bounded above by
// `n_cps_dropped_joinable`, and the gap will become meaningful
// when AC matching or extended joinability lands in 7.4+.
//
// `rule_a`/`rule_b` are indices into `s->lhs[] / s->rhs[]`.  Pass
// ATP_RULE_NONE (or any value >= n_rules) to mean "no rule
// excluded" -- equivalent to running 7.1.  The filtered rule
// arrays are heap-allocated, sized to the live n_rules, since the
// rule set is unbounded.
static u8 atp_cp_source_disjoint_connected(AtpState *s, Term lhs, Term rhs,
                                           u32 rule_a, u32 rule_b) {
  const u32 NORM_CAP = 64;
  u32 n = s->n_rules;
  Term *filt_l = (n > 0) ? (Term *)malloc((size_t)n * sizeof(Term)) : NULL;
  Term *filt_r = (n > 0) ? (Term *)malloc((size_t)n * sizeof(Term)) : NULL;
  if (n > 0 && (filt_l == NULL || filt_r == NULL)) {
    free(filt_l); free(filt_r);
    return 0;
  }
  u32 n_filt = 0;
  for (u32 k = 0; k < n; k++) {
    if (k == rule_a || k == rule_b) continue;
    filt_l[n_filt] = s->lhs[k];
    filt_r[n_filt] = s->rhs[k];
    n_filt++;
  }
  Term l = atp_rewrite_normalize(s, lhs, filt_l, filt_r, n_filt, NORM_CAP);
  Term r = atp_rewrite_normalize(s, rhs, filt_l, filt_r, n_filt, NORM_CAP);
  free(filt_l);
  free(filt_r);
  return kbo_eq(l, r);
}

// Bachmair-Dershowitz connectedness (Twee section 6.2): a critical
// pair (s, t) born from the peak `p = sigma(li)` is REDUNDANT if s and
// t can be joined through a rewrite proof in which every term is
// STRICTLY BELOW the peak in the reduction order.  Soundness: such a
// proof witnesses that the local confluence of `p` follows from
// strictly smaller overlaps already (or to be) handled, so dropping
// the CP cannot lose a needed consequence -- this is the standard
// connectedness redundancy of unfailing completion.
//
// This is STRONGER than trivial-joinability (7.1): plain normalization
// only follows order-DECREASING steps to a normal form, so two sides
// with distinct normal forms are kept; connectedness additionally
// admits an unorientable equation applied in its non-reducing local
// direction, PROVIDED the result stays strictly below the peak.  Such a
// "detour below the peak" joins CPs that 7.1 misses, which is where the
// extra pruning comes from.
//
// Implementation: a bounded forward reachability below `peak`.  Seed a
// small hash set with s and t's ordered normal forms (always < peak
// since they are proper reducts).  Repeatedly expand a frontier term by
// every one-step rewrite -- both rule directions, variable-safe -- whose
// RESULT u satisfies peak >_C u; insert u, tagged by which seed it
// descends from (s-side / t-side / both).  A term reached from BOTH
// sides is the join point -> redundant.  Bounded by ATP_CONN_MAX_NODES
// (return 0 = KEEP on overflow, so the criterion never wrongly deletes).
#define ATP_CONN_MAX_NODES 256u
// Heap cells one connectedness search may spend.  A successor at depth
// d is wrapped back through its d ancestors, so a CP thousands of levels
// deep costs O(d) cells per successor and O(d^2) per search; past the
// budget the search keeps the CP (sound).  Shallow searches stay far
// below it (256 nodes x 64 successors x a few hundred cells).
#define ATP_CONN_HEAP_CELLS (1ull << 24)
#define ATP_CONN_SIDE_S 1u
#define ATP_CONN_SIDE_T 2u

// Structural hash over the preorder (FNV-ish): structurally-equal terms
// with distinct heap cells hash identically.  Fast pre-filter before a
// kbo_eq in the connectedness reachability set.
static u32 atp_struct_hash(Term t) {
  return atp_term_fnv32(t);
}

// Collect every one-step rewrite of `t` (all positions, both
// variable-safe directions of each rule) whose result is strictly
// below `peak`, into out_buf[*n .. ).  Mirrors mnf_successors' shape
// but order-gated against the peak rather than the redex.
// The one-step rewrites of `t` at its root (every l->r, and r->l when
// its variables are contained), appended to out_buf up to `cap`.
static void atp_conn_top(AtpState *s, Term t, Term *out_buf, u32 *n,
                         u32 cap) {
  for (u32 j = 0; j < s->n_rules && *n < cap; j++) {
    RewriteSubst sub = {{0}};
    if (thvm_match(s->lhs[j], t, &sub)) {                 // l -> r
      Term repl = thvm_subst_apply(s->rhs[j], &sub);
      out_buf[(*n)++] = repl;
    }
    if (*n >= cap) break;
    if (atp_vars_contained(s->lhs[j], s->rhs[j])) {       // r -> l (var-safe)
      RewriteSubst sb = {{0}};
      if (thvm_match(s->rhs[j], t, &sb)) {
        Term repl = thvm_subst_apply(s->lhs[j], &sb);
        out_buf[(*n)++] = repl;
      }
    }
  }
}

static u32 atp_conn_successors(AtpState *s, Term t, Term peak,
                               Term *out_buf, u32 *n, u32 cap) {
  (void)peak;
  // Post-order over an explicit stack: a CTR's children's successors
  // are wrapped back into it (child i replaced) as each child finishes,
  // then the CTR's own root rewrites follow; `mark` is *n when the
  // current child began.  A node wider than REWRITE_MAX_ARITY adds
  // nothing, not even root rewrites.
  //
  // NOTE: no peak gate here.  Inner-position steps are wrapped back
  // into the parent, so a bare-reduct compare gates the WRONG term --
  // the criterion constrains the FULL rewritten term, which only the
  // BFS caller sees (it filters every successor against the peak
  // before insertion).  The earlier per-reduct gate admitted >=-peak
  // intermediates and over-dropped: McCune Associativity falsely
  // SATURATED under THVM_ATP_CONNECT=1.
  typedef struct { Term t; u64 base; u32 i; u32 m; u32 mark; } ConnFrame;
  ConnFrame  fr_inl[ATP_WALK_INL];
  ConnFrame *fr  = fr_inl;
  u32        nfr = 0u;
  u32        cfr = ATP_WALK_INL;
  for (;;) {
    u32 m    = (term_tag(t) == TAG_CTR) ? term_ctr_n(t) : 0u;
    u8  wide = (u8)(m > REWRITE_MAX_ARITY);
    if (!wide && m > 0u) {
      if (nfr == cfr) {
        fr = (ConnFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].t    = t;
      fr[nfr].base = term_val(t);
      fr[nfr].i    = 0u;
      fr[nfr].m    = m;
      fr[nfr].mark = *n;
      nfr++;
      t = heap_read(term_val(t) + 1u);
      continue;
    }
    if (!wide) {
      atp_conn_top(s, t, out_buf, n, cap);
    }
    for (;;) {
      if (nfr == 0u) {
        term_stack_release(fr, fr_inl);
        return 1u;
      }
      ConnFrame *f = &fr[nfr - 1u];
      for (u32 k = f->mark; k < *n; k++) {
        Term ch[REWRITE_MAX_ARITY];
        for (u32 c = 0; c < f->m; c++) {
          ch[c] = (c == f->i) ? out_buf[k] : term_ctr_at(f->t, c);
        }
        out_buf[k] = term_new_ctr(term_ext(f->t), ch, f->m);
      }
      f->i++;
      if (f->i < f->m) {
        f->mark = *n;
        t = heap_read(f->base + 1u + (u64)f->i);
        break;
      }
      atp_conn_top(s, f->t, out_buf, n, cap);
      nfr--;
    }
  }
}

static u8 atp_cp_connected_below_peak(AtpState *s, Term lhs, Term rhs,
                                      Term peak) {
  if (s == NULL || peak == 0) return 0;
  // Seeds = the treated CP sides as handed in.  They are already
  // engine-normalized by strictly DECREASING steps from one-step
  // reducts of the peak, hence below it.  (An earlier version
  // re-normalized here with atp_rewrite_normalize, which applies
  // UNORIENTED equations unconditionally -- steps that can pass
  // through terms at or above the peak and even fabricate the seed
  // join, over-dropping true CPs: McCuneAxioms__Associativity
  // falsely SATURATED under THVM_ATP_CONNECT=1.  Connectedness
  // demands every intermediate term strictly below the peak.)
  Term l = lhs;
  Term r = rhs;
  if (kbo_eq(l, r)) return 1;                 // trivially joined below peak
  // A seed equal to the peak (no decreasing step taken) cannot stay
  // strictly below it -- bail (KEEP) to preserve soundness.
  if (atp_compare(s, peak, l) != KBO_GT || atp_compare(s, peak, r) != KBO_GT) {
    return 0;
  }
  Term  terms[ATP_CONN_MAX_NODES];
  u32   hash [ATP_CONN_MAX_NODES];
  u8    side [ATP_CONN_MAX_NODES];
  u32   n = 0u;
  terms[n] = l; hash[n] = atp_struct_hash(l); side[n] = ATP_CONN_SIDE_S; n++;
  terms[n] = r; hash[n] = atp_struct_hash(r); side[n] = ATP_CONN_SIDE_T; n++;
  Term succ[64];
  u64  h0 = HEAP_NEXT;
  for (u32 cur = 0; cur < n; cur++) {
    u32 ns = 0u;
    (void)atp_conn_successors(s, terms[cur], peak, succ, &ns, 64u);
    if (HEAP_NEXT - h0 > ATP_CONN_HEAP_CELLS) {
      return 0;                              // over budget -> KEEP (sound)
    }
    for (u32 k = 0; k < ns; k++) {
      Term u  = succ[k];
      // The criterion's load-bearing gate: every term on the
      // connecting chain must sit STRICTLY below the peak.
      if (atp_compare(s, peak, u) != KBO_GT) continue;
      u32  hu = atp_struct_hash(u);
      u32  found = ATP_CONN_MAX_NODES;
      for (u32 i = 0; i < n; i++) {
        if (hash[i] == hu && kbo_eq(terms[i], u)) { found = i; break; }
      }
      if (found != ATP_CONN_MAX_NODES) {
        if ((side[found] | side[cur]) == (ATP_CONN_SIDE_S | ATP_CONN_SIDE_T)) {
          return 1;                          // join point reached from both
        }
        side[found] = (u8)(side[found] | side[cur]);
        continue;
      }
      if (n >= ATP_CONN_MAX_NODES) return 0;  // overflow -> KEEP (sound)
      terms[n] = u; hash[n] = hu; side[n] = side[cur]; n++;
    }
  }
  return 0;
}

// Stage 7.3a: rule subsumption check.  Returns 1 if there exist a
// rule `(l_k, r_k) ∈ R` and a substitution σ such that
// `(lhs, rhs) = (σ l_k, σ r_k)` (forward) or `(lhs, rhs) =
// (σ r_k, σ l_k)` (symmetric).  Equational subsumption: the σ must
// be CONSISTENT across both sides simultaneously, so we extend the
// same `RewriteSubst` across the two `thvm_match` calls.
//
// Per the domination lemma in `docs/plans/connectedness_design.md`:
// if (lhs, rhs) is rule-subsumed by (l_k, r_k), then rule
// (l_k, r_k) rewrites lhs to rhs in one step under σ, so
// `thvm_rewrite_normalize` collapses the pair too.  Hence
// `n_cps_dropped_rule_subsumed <= n_cps_dropped_joinable` always.
// We tick the counter for empirical measurement; the filtering
// itself stays in 7.1.
//
// 7.3b will add queue subsumption -- which IS orthogonal to 7.1.
static u8 atp_cp_rule_subsumed(AtpState *s, Term lhs, Term rhs) {
#if defined(THVM_ATPFT_RULES) && defined(THVM_ATPFT_MATCH)
  // FT path: ft_match against the lhs_ft[]/rhs_ft[] mirror.
  // Per-CP-push this was ~200 thvm_match samples in the post-FT-skip
  // profile; ft_match avoids the heap-cell descent.  Requires the
  // mirror to be fully populated (test setups that bypass
  // atp_push_rule write s->lhs[] directly and leave lhs_ft[] NULL).
  // Mirror status is cached on AtpState by atp_cp_trivially_joinable.
  if (s->ft_mirror_full && s->n_rules <= s->ft_mirror_probed_n_rules) {
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *lhs_ft_in = ft_from_term(a, lhs, /*scratch=*/1);
    AtpFtCell *rhs_ft_in = ft_from_term(a, rhs, /*scratch=*/1);
    AtpFtSubst subst;
    u8 hit = 0u;
    for (u32 k = 0; k < s->n_rules; k++) {
      if (s->r_dead != NULL && s->r_dead[k]) continue;
      AtpFtCell *rl = s->lhs_ft[k];
      AtpFtCell *rr = s->rhs_ft[k];
      if (rl == NULL || rr == NULL) continue;  // defensive
      // Forward: σ rl = lhs AND σ rr = rhs (one σ extended via the
      // thread-through semantics of ft_match -- it leaves bindings
      // from a successful match in place for the next call).
      memset(&subst, 0, sizeof(subst));
      if (ft_match(rl, lhs_ft_in, &subst) &&
          ft_match(rr, rhs_ft_in, &subst)) { hit = 1u; break; }
      // Symmetric.
      memset(&subst, 0, sizeof(subst));
      if (ft_match(rl, rhs_ft_in, &subst) &&
          ft_match(rr, lhs_ft_in, &subst)) { hit = 1u; break; }
    }
    ft_scratch_reset(a);
    return hit;
  }
#endif
  for (u32 k = 0; k < s->n_rules; k++) {
    // Forward: σl_k = lhs AND σr_k = rhs (one σ extended through
    // both matches).
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[k], lhs, &subst) &&
          thvm_match(s->rhs[k], rhs, &subst)) {
        return 1;
      }
    }
    // Symmetric: σl_k = rhs AND σr_k = lhs.
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[k], rhs, &subst) &&
          thvm_match(s->rhs[k], lhs, &subst)) {
        return 1;
      }
    }
  }
  return 0;
}

// WM SubsumptionBody core (INF/Subsumption.c:67-89 wrapping
// SS_TermpaarSubsummiertTermpaar's test, :104-110): does the equation
// (p_lhs, p_rhs) subsume the pair (lhs, rhs) "at some position"?  The
// pair itself, or the subpair reached by repeatedly descending into
// the UNIQUE differing immediate subterm (both sides must share the
// top symbol and agree on every other child, else FAIL --
// TO_TopSymboleGleich / NachfolgendeTeiltermeGleich), is an instance
// of the pattern pair under ONE substitution covering both sides
// (MO_TermpaarSubsummiertZweites, MatchOperationen.c:1452-1505:
// SubstAufIdentitaetSetzen runs once and the binding persists across
// the left- and right-side loops).  Both orientations of the PATTERN
// are tried (Subsumption.c:108-109).
static u8 atp_eq_subsumes_pair(Term p_lhs, Term p_rhs, Term lhs, Term rhs) {
  for (;;) {
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(p_lhs, lhs, &subst) &&
          thvm_match(p_rhs, rhs, &subst)) {
        return 1;
      }
    }
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(p_lhs, rhs, &subst) &&
          thvm_match(p_rhs, lhs, &subst)) {
        return 1;
      }
    }
    // Descend one level: same top symbol + exactly one differing
    // child, else no position can be subsumed.
    if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
    if (term_ext(lhs) != term_ext(rhs)) return 0;
    u32 n = term_ctr_n(lhs);
    if (term_ctr_n(rhs) != n) return 0;
    u32 diff = n;
    for (u32 i = 0; i < n; i++) {
      if (!kbo_eq(term_ctr_at(lhs, i), term_ctr_at(rhs, i))) {
        if (diff != n) return 0;        // a second differing child
        diff = i;
      }
    }
    if (diff == n) return 0;            // identical pair: unreachable
    lhs = term_ctr_at(lhs, diff);
    rhs = term_ctr_at(rhs, diff);
  }
}

