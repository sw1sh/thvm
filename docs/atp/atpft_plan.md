# AtpFt: ATP-Private Flatterm Data Structure

ATP is its own venture, separate from the heap-cell `Term` used by
IC / UOP / WL. This plan stands up a native flatterm representation
(`AtpFtCell`) modeled on Waldmeister's `TermzellenT`, with its own
arenas and operations, so the ATP rewrite/match/normalize stack
operates on AtpFt directly and only converts to heap-cell Term at
narrow boundaries (WL bridge in/out, trace, pretty-print).

See `docs/atp/wm_native_cli_recipe.md.scoreboard` iter 200+ for the
context that motivated this plan (the push-norm 10x gap to WM that
the prior structural ports could not close because they were always
fronting a Term-cell representation).

---

## Cell layout (24 B, 8-byte aligned)

```c
typedef struct AtpFtCell {
  struct AtpFtCell *next;   // 8B: next cell in pre-order walk (WM Nachf)
  struct AtpFtCell *end;    // 8B: last cell of THIS subterm    (WM Ende)
  u32               sym;    // 4B: sym (high bit = var flag)
  u16               arity;  // 2B: 0..REWRITE_MAX_ARITY (cached -- avoids
                            //     heap_read per visit; biggest single
                            //     reason current FLAT cp-gen lags TREE)
  u8                flags;  // 1B: bit0=subst_fresh, bit1=ground,
                            //     bit2=lpo_rank_valid
  u8                _pad;
} AtpFtCell;
```

Field order chosen so `cell->end->next` is the standard
"next-sibling-of-this-subterm" jump (mirrors WM `TO_NaechsterTeilterm`
and matches the offsets that thvm's existing `KboFlatNode` uses --
LPO/KBO fast paths can take `AtpFtCell*` directly with zero encode).

`subst_fresh` = WM substFlag (innermost-rewrite skip).
`ground` = OR-folded at construction by `ftnew_ctr` so LPO has an
O(1) ground test it lacks today.

## Two-arena allocator

* **Arena A -- persistent.** Slab pool, 30000 cells/block (WM
  number) = 720 KB per block, free-list per slab. Holds anything
  reachable from a root (`s->lhs[i]`, `s->rhs[i]`, queued CPs,
  witness bindings, MNF colored nodes).
  Operations: `ft_alloc_persistent`, `ft_free_span(first, last)`
  (O(1) chain push -- mirrors WM `TermzellenlisteLoeschen`).

* **Arena B -- scratch.** Bump pointer in one contiguous region
  (256 KB to start, doubles on overflow). Reset O(1) at the top of
  every push-norm / cp-gen call. Holds subject flatten, RHS rebuild
  scratch, narrow/match temporaries.

GC stays exactly as today: walks roots, but via `ft_walk_persistent`
instead of `term_walk`. Scratch is invisible to GC.

## Primitive API

```c
// Construction (Arena A default; scratch=1 routes to Arena B)
AtpFtCell *ftnew_var  (AtpFt *a, u32 var_id,            int scratch);
AtpFtCell *ftnew_const(AtpFt *a, u32 sym,               int scratch);
AtpFtCell *ftnew_ctr  (AtpFt *a, u32 sym, u16 arity,
                       AtpFtCell *const *kids,          int scratch);

// Per-call substitution slot table (NOT in-cell -- shared rule LHSs
// across matches would otherwise need per-match clearing)
typedef struct { AtpFtCell *bind[REWRITE_MAX_VAR]; u32 wm; } AtpFtSubst;
int  ft_match      (const AtpFtCell *pat, const AtpFtCell *subj, AtpFtSubst *o);
AtpFtCell *ft_subst_apply(AtpFt *a, const AtpFtCell *tmpl,
                          const AtpFtSubst *s, int scratch);

// In-place rewrite at a position
AtpFtCell *ft_splice(AtpFt *a, AtpFtCell *root,
                     AtpFtCell *redex_vorg, AtpFtCell *redex,
                     const AtpFtCell *rhs_tmpl, const AtpFtSubst *s);

// Equality + hash (struct-keyed)
int  ft_eq  (const AtpFtCell *x, const AtpFtCell *y);
u64  ft_hash(const AtpFtCell *x);

// Boundary
AtpFtCell *ft_from_term(AtpFt *a, Term t, int scratch);
Term       ft_to_term  (const AtpFtCell *x);
```

## CP queue layout

Replaces `acp_pack`/`acp_unpack` varint encoding:

```c
typedef struct {
  AtpFtCell *lhs;
  AtpFtCell *rhs;
  u32        size;
  u32        weight;
  u16        priority;
  u16        origin_rule;
} AtpCpEntry;
```

24 B header + cells (Arena A). ~144 B per 5-cell CP vs acp_pack's
~12 B -- 10x larger storage, but eliminates the unpack + heap-Term
materialize on every `select_cp` (the dominant cost we measured).

## LPO/KBO: AtpFt is the flatterm

The existing `KboFlatNode` is exactly `{u32 sym; u32 next_sib; u16
arity; ...}` -- a flatterm with sibling-skip. `AtpFtCell` is the
same layout with the field order chosen so `cell->end->next` IS the
next-sibling jump. Net: `thvm_kbo`/`thvm_lpo` accept `AtpFtCell*`
with no encode step (Stage 3).

---

# 10-Stage Migration

Each stage is a separate commit, opt-in flag, leaves test_atp
135624/135624 green. Stages in dependency order. Breakthrough at
Stage 6.

### Stage 1 -- cell + dual-arena allocator + slab pool
- Scope: new `src/atp/ft.h` (cell struct) + `src/atp/ft_alloc.c`
  (slab pool, bump scratch, `ft_alloc_persistent`, `ft_free_span`,
  `ft_scratch_reset`, `ft_walk_persistent`). No live callers yet.
  Unit test `tests/test_ft_alloc.c`.
- Flag: `THVM_ATPFT_ALLOC` build define.
- LOC: ~350. Risk: LOW (additive).
- Accept: `test_ft_alloc` green; `test_atp` 135624/135624.
- Push-norm impact: none.

### Stage 2 -- constructors + boundary converters + struct-eq/hash
- Scope: `ft.c` -- `ftnew_var/const/ctr` (stitches next/end,
  OR-folds ground, caches arity); `ft_from_term`/`ft_to_term`;
  `ft_eq`/`ft_hash` paralleling `atp_term_struct_hash`;
  `atp_pretty_ft` mirror of `atp_pretty_term`. Test: 10k random
  Terms round-trip via from/to and hash agrees with reference.
- Flag: `THVM_ATPFT_CONVERT` build define.
- LOC: ~450. Risk: LOW (pure converters, differential-tested).
- Push-norm impact: none.

### Stage 3 -- AtpFt-native LPO/KBO entry points
- Scope: add `thvm_kbo_ft(AtpFtCell*, AtpFtCell*)` and
  `thvm_lpo_ft(...)` reading cells directly. `thvm_kbo(Term,Term)`
  becomes a wrapper that calls `ft_from_term` (Arena-B scratch).
  Validate via existing `test_kbo*`/`test_lpo*`/`test_probe_lpo_wm`.
- Flag: `THVM_ATPFT_LPO=1` env.
- LOC: ~400. Risk: LOW (LPO/KBO already flatterm-internal).
- Push-norm impact: none (LPO is in cp-priority).

### Stage 4 -- rule storage migration (s->lhs/rhs -> AtpFtCell*)
- Scope: `AtpState.lhs/rhs/r_dead_lhs_save/r_dead_rhs_save` become
  `AtpFtCell **`. Update `atp_orient_and_add` and GC roots in
  `thvm_atp_gc_collect`. Parallel `atp_ri_descend_ft` for the
  rule index reading `cell->sym`/`cell->next`.
- Flag: `THVM_ATPFT_RULES=1` env.
- LOC: ~700. Risk: MED (GC roots).
- Accept: `test_atp` green under both flag values; AndAssoc 30s
  bench cp count matches within 0.1%.
- Push-norm impact: partial (kills per-call rule flatten under
  FLAT path).

### Stage 5 -- AtpFt-native match + slot-table substitution
- Scope: `ft_match.c` -- `ft_match` (pattern vs subject ->
  `AtpFtSubst.bind[REWRITE_MAX_VAR]` + wm watermark) and
  `ft_subst_apply` (builds into Arena B). Differential test
  against `thvm_match` over 10^5 cases. Wire into
  `atp_ri_descend_ft` and `atp_overlap_ij`.
- Flag: `THVM_ATPFT_MATCH=1` env (requires Stage 4).
- LOC: ~600. Risk: MED (match correctness).
- Push-norm impact: partial (slot-table backtrack, no per-match
  clear pass).

### Stage 6 -- AtpFt-native splice + subst_fresh push-norm  *** BREAKTHROUGH ***
- Scope: `ft_splice.c` -- `ft_splice` doing the WM
  `SigmaRInEZ`/`InLZ` split (build instantiated RHS in Arena B,
  in-place splice into Arena A via next/end rewire, free displaced
  span). Set `subst_fresh` on every freshly-spliced cell. New
  `atp_rewrite_normalize_ft_fixpoint` consuming the bit (skip
  cells with `subst_fresh==1` in inner-rewrite; clear after
  fixpoint -- the WM `BL_NormalformInnermost2` trick).
- Flag: `THVM_ATPFT_NORM=1` env (requires Stage 5).
- LOC: ~750. Risk: MED.
- Accept: `test_atp` green; pos-memo redundancy drops below 20%
  (currently 91%).
- Push-norm impact: **15.8 us/cp -> ~3-4 us/cp** (the WM trick
  that closes most of the gap).

### Stage 7 -- AtpFt CP queue (replace acp_pack)
- Scope: replace `acp_pack`/`_unpack` + `cp_packed[]` with
  `AtpCpEntry` rooted in Arena A. Update queue push/pop,
  `select_cp`, `cp_get/set`, `peek_top_k`, `cp_features`.
- Flag: `THVM_ATPFT_CPQ=1` (requires Stage 6).
- LOC: ~550. Risk: MED.
- Accept: AndAssoc 30s -- live-CP timeline, selection order, and
  final proof byte-identical.
- Push-norm impact: partial (~1 us/cp from no re-flatten on
  select_cp).

### Stage 8 -- precedence.c + weight/priority paths
- Scope: `src/atp/precedence.c` ports + `_.c` weight/priority
  sites (`atp_kbo_weight`, `atp_cp_priority`, `atp_goal_*`,
  `atp_term_depth`, etc.) read AtpFt cells directly.
- Flag: `THVM_ATPFT_PRIO=1` (requires Stage 7).
- LOC: ~500. Risk: LOW.
- Push-norm impact: partial (kills per-call flatten on weight/
  priority path that fires 22.6M ri-queries/run).

### Stage 9 -- MNF + hash memos + trace-arg cleanup
- Scope: re-key `atp_unf_memo`, `atp_norm_memo`, `mnf_lookup/insert`,
  `atp_struct_hash` with `ft_hash`/`ft_eq`. Trace and proof-step
  `before/after` stay heap Term (cold, GC-rooted) but converted
  via `ft_to_term` at recording site.
- Flag: `THVM_ATPFT_MEMOS=1` (requires Stage 8).
- LOC: ~400. Risk: LOW.
- Push-norm impact: partial (~0.5 us/cp -- kills Term materialize
  on memo probes).

### Stage 10 -- cutover
- Scope: flip defaults of all `THVM_ATPFT_*` flags to ON. Keep
  Term-path source + `THVM_ATPFT_DISABLE=1` escape hatch for one
  release. Update scoreboard. Delete dead `acp_pack`/`_unpack`
  in a follow-up.
- Flag: `THVM_ATPFT_DISABLE=1` reverses.
- LOC: ~150. Risk: LOW.
- Accept: AndAssoc 30s push-norm <= 2.5 us/cp; mccune cracker
  push-norm <= 2.5 us/cp; `test_atp` 135624/135624.

---

## Summary

**10 stages, ~4850 LOC total**: 650 infra + 1300 LPO/match/splice +
1050 rules/queue + 900 priority/memos + 950 cleanup/cutover.

**Breakthrough at Stage 6** when `subst_fresh` + in-place splice +
slot-table backtrack collapse the per-cell allocation tax dominating
the current 15.8 us/cp push-norm.

Stages 7-9 trim the remaining 30-40%.

Each preceding stage individually leaves test_atp 135624/135624
green and ships behind its own env / build flag, so a regression
rolls back to the prior tested configuration without losing the
intervening commits.
