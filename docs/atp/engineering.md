# thvm/atp — Engineering

How the algorithms in [algorithms.md](algorithms.md) map to thvm's
data structures, what's hot, what's gated.  Read algorithms.md first
for the math.

## Contents

* [Term representation](#term-representation)
* [AtpState](#atpstate)
* [Rule storage](#rule-storage)
* [CP queue](#cp-queue)
* [Discrimination tree](#discrimination-tree)
* [Memos and caches](#memos-and-caches)
* [AtpFt](#atpft)
* [Controls](#controls)
* [Soundness probes](#soundness-probes)
* [Profiling](#profiling)

## Term representation

The engine operates on heap-cell `Term`s — thvm's universal 64-bit
packed pointer into a copying IC heap (shared with the rest of
thvm — IC reduction, UOP, WL bridge).  A `Term` is one of `TAG_CTR`
(constructor with label + arity + child cells), `TAG_FVR` (free
variable with a 32-bit id), `TAG_NUM` (numeric leaf), plus a few
others that don't appear in ATP-internal terms.  Accessors:
`term_tag(t)`, `term_ext(t)` (label / var id / dtype), `term_val(t)`
(child base or numeric value), `term_ctr_n(t)` (arity),
`term_ctr_at(t, i)` (child).

Heap cells are GC-relocated by a Cheney collector; the ATP engine
participates as a root provider (`thvm_atp_gc_collect` in
`src/atp/_.c:4434`) and invalidates its term-keyed memos on every
GC.

The engine also carries an alternative cell-list representation
([AtpFt](#atpft)) modeled on Waldmeister's `TermzellenT`, in which
each cell carries `next` (next-in-preorder) and `end` (end-of-
subterm) pointers.  AtpFt cells live in a dedicated slab allocator
and are GC-invisible.  Both representations co-exist; rule storage
under `THVM_ATPFT_RULES` keeps the two in lockstep.  AtpFt is
gated, opt-in, and used by the parallel match / splice / normalize
/ rule-index-descent / CP-queue paths.

## AtpState

`AtpState` (in `src/thvm.h:3174..4125`) is the single per-engine
struct.  The big fields:

```
// rules — append-only with periodic compaction; index-stable
// between compactions
Term       *lhs, *rhs;
u8         *r_orient;   // 1 = lhs > rhs under ordering; 0 = unorient
u8         *r_dead;     // 1 = retired by interreduction
u32        *r_trace;    // back-pointer into proof trace
Term       *r_dead_lhs_save, *r_dead_rhs_save;  // pre-retirement state
u32         n_rules, n_unorient, cap_rules;

// CP queue (min-heap on cp_pri)
u8        **cp_packed;  // varint-encoded CP, one byte string per slot
u32        *cp_pri;     // priority (heap key)
u32        *cp_seq;     // monotonic insertion counter (FIFO tiebreak)
u32        *cp_goal;    // goal-relevance score
u32        *cp_pri2;    // K-D Heap secondary key
u32        *cp_trace;   // back-pointer into proof trace
u32         n_cps, cap_cps;

// indexes
struct AtpRuleIndex  *rule_index;       // orientable rule LHSs
struct AtpRuleIndex  *unorient_index;   // unorientable equation faces
struct AtpFvIndex    *fv_index;         // CP feature vectors
struct AtpRuleIndex  *cp_index;         // optional CP-subterm index
struct AtpRuleIndex  *cp_subindex;

// goal
Term goal_lhs, goal_rhs, goal_lhs_nf, goal_rhs_nf;
u8   goal_existential;
RewriteSubst witness_subst;

// trace (proof DAG)
Term  *trace;
u32    n_trace, t_max;

// MNF (Mandatory Normal Form) — opt-in eager normalization
u8     use_mnf;
u32    mnf_color_count;
// ... mnf_* fields, ~40 of them ...

// AtpFt mirrors (under THVM_ATPFT_RULES)
struct AtpFt        *ft_arena;
struct AtpFtCell   **lhs_ft, **rhs_ft;
struct AtpFtCell   **r_dead_lhs_save_ft, **r_dead_rhs_save_ft;
struct AtpFtCell    *goal_lhs_ft, *goal_rhs_ft;

// AtpFt CP queue (under THVM_ATPFT_CPQ)
void *cp_packed_ft;   // AtpCpEntry[] parallel to cp_packed

// ordering configs
const KboConfig *kbo;
const LpoConfig *lpo;

// + ~80 more knobs / counters / mode flags
```

Init: `thvm_atp_init(kbo_cfg, step_cap)` mallocs and calls
`atp_ensure_rule_cap` for the initial slot capacity.  Destroy:
`thvm_atp_free` frees the arrays and AtpFt arenas.

## Rule storage

Append-only.  `atp_push_rule(s, lhs, rhs)` (`src/atp/_.c:6723`)
writes `s->lhs[n_rules] = lhs; s->rhs[n_rules] = rhs;
r_orient[n_rules] = (atp_compare(s, lhs, rhs) == KBO_GT)`.
Unorientable rules increment `n_unorient`.  Marks the rule index
dirty so the next descent rebuilds.

Retirement happens during interreduction (`src/atp/_.c:6452`,
`:8746`, `:8888`, `:8918`).  Two flavors:

* **LHS-collapse.** The new rule rewrites this rule's LHS, so this
  rule is no longer in normal form.  Save the LHS/RHS into the
  `r_dead_*_save` slots (for proof reconstruction), shift the live
  arrays down, decrement `n_rules`.
* **Right-reduce.** The new rule rewrites this rule's RHS in place.
  Update `s->rhs[i]` to the reduced form; the rule stays.

The dead-save arrays preserve the original equation so the trace
can re-derive the retired rule's contribution.

### Rule index dirtying

Any rule storage mutation sets `s->rule_index_dirty = 1` and
`s->wmfpa_dirty = 1`.  The discrim tree rebuilds at the next
`atp_unorient_step_indexed` or `atp_rewrite_normalize_indexed` call.
All three rebuild sites (the two above plus
`atp_rewrite_normalize_flatterm_mixed`) invariantly call
`atp_unf_memo_invalidate()` after rebuild so the no-fire pos-memo
cannot outlive an R change.

## CP queue

The queue is a min-heap on `cp_pri[]` with `cp_seq[]` as a stable
FIFO tiebreaker.  Each slot `i` stores:

* `cp_packed[i]` — pointer to a malloc'd byte string that encodes
  the CP's two terms in pre-order varint format (`acp_pack` in
  `src/atp/_.c:864`).  1 discriminator byte + LEB128 varints per
  symbol.  Self-delimiting (the unpacker walks pre-order via
  discriminator + arity until two terms are reconstituted).
* `cp_pri[i]`, `cp_seq[i]`, `cp_goal[i]`, `cp_pri2[i]` — the four
  parallel heap-key arrays.
* `cp_trace[i]` — trace index for the CP's two parent rules.

The byte-string encoding is small (avg ~12 B per CP) and survives
GC unchanged because it contains no Term cell pointers — just symbol
labels and var ids.  Heap insertion is the standard sift-up/sift-
down in `atp_cp_swap` + `atp_cp_sift_*`.

Pop (`thvm_atp_select_cp`, `src/atp/_.c:6304`) decodes via
`acp_unpack` into two fresh heap Terms.  Periodic non-min picks
(FIFO/random/goal/K-D) replace the min-pop with a linear scan over
the queue.

### AtpFt CP queue mirror

Under `THVM_ATPFT_CPQ`, a parallel `AtpCpEntry[]` array (`src/atp/
ft_cpq.c`) holds the same CPs as native AtpFt cell-pairs (32 B
header + cells in Arena A).  Decode-free pop; trivially-joinable
check operates on `AtpFtCell *` directly via
`atp_cp_trivially_joinable_ft`.

## Discrimination tree

`AtpRuleIndex` (`src/atp/_.c:2153..2698`) is a left-child / right-
sibling tree with a single `sym : u32` edge label per node, drawn
from three disjoint ranges:

* `[CTR_BASE, CTR_BASE + n_labels)` — exact constructor label.
* `NUM` — any numeric leaf (collapsed to one symbol).
* `[STAR_BASE, STAR_BASE + ATP_RI_MAXVARS)` — a rule variable, with
  the *first-appearance* var-renumbering.  Distinct rule vars get
  distinct STAR edges, so the tree distinguishes `f(x,x)` from
  `f(x,y)` structurally.

`atp_ri_rebuild` (`src/atp/_.c:2368`) walks every live rule LHS in
pre-order and inserts.  Leaves carry an `AtpRiRec` list pointing
back into `s->lhs[]`.

Descent (`atp_ri_descend` for orientable, `atp_ri_descend_unorient`
for unorientable) walks the subject's pre-flattened arrays
position-by-position.  STAR edges bind a subject preorder position
into `g_atp_ri_star[k]`; repeat-var consistency is a `memcmp` over
the bound slice's flatsym.  Backtrack is implicit (recursive
descent's call frames undo the bind).  At a leaf, if `perfect =
!ix->any_folded && !g_atp_ri_query_folded` then arriving at the
leaf IS the match proof; otherwise the descent falls through to a
`thvm_match` against the stored pattern.

**Var folding.** `ATP_RI_MAXVARS = 64` is the per-rule var-slot
budget.  A rule with more distinct vars gets its high-id vars
"folded" onto the last slot, making the tree coarser; the leaf
then re-runs `thvm_match` as the authoritative guard.

`atp_ri_descend_ft_at` in `src/atp/ft_ri.c` is the AtpFt-pointer-
walking parallel.  The tree itself is representation-agnostic
(symbol-skeleton encoding); only the terminal `ft_eq` consistency
check differs.

### FV index

`AtpFvIndex` (`src/atp/_.c:1500..1738`) keys on a per-CP feature
vector — symbol-multiset, depth, var count, etc. — and returns CPs
whose FVs are componentwise-dominated by the query.  Used by
`atp_cp_queue_subsumed` to find CPs the new one might be a
substitution instance of.  Cheaper than walking the whole queue.

## Memos and caches

The engine carries several memos, each with its own invalidation
discipline:

| Memo                       | Key                                | Invalidation                  |
|----------------------------|------------------------------------|-------------------------------|
| `g_kbo_wmemo`              | (Term, epoch)                      | `thvm_kbo_invalidate()` (GC)  |
| `g_lpo_memo`               | (Term, Term, epoch)                | `thvm_lpo_invalidate()` (GC)  |
| `g_atp_unf_memo`           | `atp_term_struct_hash(t)`          | every rule_index rebuild      |
| `g_atp_unf_pos_memo`       | (subtree_phash, folded bit)        | every rule_index rebuild      |
| `g_atp_norm_cache`         | `atp_term_struct_hash(t)` + R-ep   | on R change                   |
| `g_atp_orient` (opt-in)    | (lhs_hash, rhs_hash, epoch)        | on KBO/LPO precedence change  |

All are direct-mapped, epoch-stamped (so invalidation is `epoch++`,
O(1) until wrap).  Soundness depends on the invalidation hook
firing on every R-change for memos keyed on R-dependent verdicts.
The `atp_unf_memo` family bumps its epoch from inside
`atp_ri_rebuild`'s prologue at every rebuild site.

**Memo overlap.** A new top-level cache on top of an already-
deduplicating bottom-level memo adds overhead.  The LPO
orientability cache (`src/atp/lpo_cache.c`, env
`THVM_ATP_LPO_ORIENT_CACHE`) sits above `kbo_subtree_memo`; its
top-level (lhs_hash, rhs_hash) hit rate is high (99% on AndAssoc)
but the existing `kbo_subtree_memo` already deduplicates most of
the work, so the live AndAssoc bench shows the orient cache as
neutral-to-slightly-negative.  TODO: re-key `kbo_subtree_memo`
itself from Term-pointer to shape-hash, which would lift its 35%
hit rate toward 70% and let the orient cache fall back to its
intended role.

## AtpFt

`src/atp/ft*.c` is a parallel native-flatterm representation modeled
on Waldmeister's `TermzellenT`.  Each cell is 24 B (8-byte aligned),
with `next` / `end` pointers carrying the pre-order linkage and a
sibling-skip:

```c
typedef struct AtpFtCell {
  struct AtpFtCell *next;   // 8B: next cell in pre-order walk
  struct AtpFtCell *end;    // 8B: last cell of this subterm
  u32               sym;    // 4B: high bit = var, low 31 = id/label
  u16               arity;  // 2B: cached
  u8                flags;  // 1B: SUBST_FRESH, GROUND, LPO_RANK_VALID
  u8                _pad;
} AtpFtCell;
```

`cell->end->next` gives the next sibling.  `cell->sym & 0x80000000`
flags variables.

### Arenas

Two arenas (`src/atp/ft_alloc.c`):

* **Persistent (Arena A).** 30000-cell slab pool, free-list per
  slab.  Cells are stable across allocations (slabs malloc'd once
  and never realloc'd).  Holds anything reachable from a root:
  `s->lhs_ft[i]`, `s->rhs_ft[i]`, AtpFt-mirror CP queue entries.
  `ft_alloc_persistent` pops the free list (refills with a fresh
  slab on empty); `ft_free_span(first, last)` is O(1) chain-push
  (mirrors WM's `TermzellenlisteLoeschen`).

* **Scratch (Arena B).** Bump-pointer over a single contiguous
  region (256 KB initial, realloc 2x on overflow).
  `ft_scratch_reset` is one `top := base` assignment.  Used for
  per-call subject flatten in the boundary converter.

### Operations

The AtpFt module exports:

* **Constructors** (`src/atp/ft.c`): `ftnew_var`, `ftnew_const`,
  `ftnew_ctr`.  Stitch `next`/`end` pointers, OR-fold the GROUND
  flag.
* **Converters**: `ft_from_term` (Term -> AtpFt), `ft_to_term`
  (AtpFt -> Term).  O(|term|).
* **Equality + hash**: `ft_eq` (structural), `ft_hash` (byte-
  identical FNV-64 recurrence with `atp_term_struct_hash`, so a
  Term and its `ft_from_term` image hash to the same u64).
* **Pretty-print**: `atp_pretty_ft` (byte-identical output to
  `atp_pretty_term`).
* **Ordering** (`src/atp/ft_order.c`): `thvm_kbo_ft`, `thvm_lpo_ft`
  flatten cells into `KboFlatNode[]` once per call and dispatch
  to the existing `thvm_kbo_flat_slice` / `lpo_flat_rec_compute`.
* **Match + subst-apply** (`src/atp/ft_match.c`): `ft_match`
  (pattern vs subject -> `AtpFtSubst` slot table with watermark
  backtrack).  `ft_subst_apply` builds the instantiated template
  into Arena B.
* **Splice** (`src/atp/ft_splice.c`): WM `SigmaRInEZ`/`InLZ`-style
  in-place rewrite at a redex position.  Marks every freshly
  emitted cell with `ATPFT_FLAG_SUBST_FRESH`; the next
  normalize-fixpoint pass skips those cells (WM's
  `BL_NormalformInnermost2` innermost-skip trick).
* **Normalize** (`src/atp/ft_norm.c`): `atp_rewrite_normalize_ft`
  fixpoint.  Entry-clear of all `SUBST_FRESH` bits; loop redex-
  find + splice; clear at exit.
* **Discrim-tree descent** (`src/atp/ft_ri.c`):
  `atp_ri_descend_ft_at` is the AtpFt-pointer-walking parallel to
  `atp_ri_descend_rec`.  The tree itself is shared with the Term
  side (symbol-skeleton encoding is representation-agnostic); only
  the terminal `ft_eq` consistency check differs.
* **CP queue** (`src/atp/ft_cpq.c`): `AtpCpEntry { lhs, rhs; size;
  weight; priority; origin_rule; }` array parallel to
  `cp_packed[]`.  Native joinability via
  `atp_cp_trivially_joinable_ft` — `atp_rewrite_normalize_ft` on
  both sides + `ft_eq` for the verdict, no `ft_from_term` at entry,
  no `ft_to_term` at exit.

### Status

The AtpFt path is byte-equivalent to the Term path: the full
135624-assertion ATP regression suite passes under every flag
combination, including `THVM_ATPFT_NORM_VERIFY=1` (live
differential — every push-norm runs both paths and aborts on
disagreement).

On AndAssoc bench (sound canonical config), per-CP push-norm under
the AtpFt stack is ~14us, essentially equal to the Term path.
The bottleneck on this workload is `atp_ft_unorient_step` — 96% of
its calls return no fire and the cost is the discrim-tree query
itself, not anything either representation can lift.  TODO: this
is a CP-selection / search-strategy issue, not a representation
issue; it lives under [roadmap.md](roadmap.md).

## Controls

The engine's behavior is shaped through three layers of controls:

* **Build defines** (the AtpFt + cache opt-ins).
* **Public C setters** on `AtpState` (the Method-level surface the
  WL paclet wraps; documented in
  `wl/THVMLink/docs/Tutorials/AtpMethods.md`).
* **Env knobs** (the bench / debug surface; same effect as setters
  but read by the test harness so a saturation can be reshaped from
  the shell).

This section catalogs the **C-level controls** — what exists below
the WL `Method` surface.  For the WL `Method` / preset / portfolio
surface, see `wl/THVMLink/docs/Tutorials/ATP.md` and
`AtpMethods.md`.

### Build defines

Set via `-D` at compile time (`CFLAGS` or per-target Makefile rule).

| Define                          | Enables                                                       |
|---------------------------------|----------------------------------------------------------------|
| `THVM_ATPFT_ALLOC`              | AtpFt arena allocator + `ft.h` cell type                      |
| `THVM_ATPFT_CONVERT`            | `ftnew_*`, `ft_from/to_term`, `ft_hash`, `ft_eq`              |
| `THVM_ATPFT_LPO`                | `thvm_kbo_ft` + `thvm_lpo_ft`                                  |
| `THVM_ATPFT_MATCH`              | `ft_match` + `ft_subst_apply`                                  |
| `THVM_ATPFT_RULES`              | `s->lhs_ft / rhs_ft` dual-store                                |
| `THVM_ATPFT_NORM`               | `atp_rewrite_normalize_ft` + `ft_splice` in join check         |
| `THVM_ATPFT_RI`                 | `atp_ri_descend_ft` (cell-walking discrim tree)                |
| `THVM_ATPFT_CPQ`                | native AtpFt CP queue alongside byte queue                     |
| `THVM_ATP_LPO_ORIENT_CACHE`     | LPO/KBO orientability cache (top-level)                        |
| `THVM_ATP_AC`                   | AC declarations + canonical form + AC-eq trivial-join hook    |
| `ATP_RULE_INDEX`                | discrim tree over rule LHSs (default on)                       |
| `ATP_FV_INDEX`                  | FV index over queued CPs (default on)                          |
| `ATP_CP_GROUND_JOIN`            | ground-joinability redundancy filter                            |
| `ATP_VAR_NORM`                  | canonicalize variable ids at CP push time                       |
| `ATP_ORDERED_REWRITE`           | order-gated rewriting (vs the linear scan)                      |
| `ATP_FLATTERM_SELFCHECK`        | per-call flatterm-vs-tree NF assertion (build-only)             |
| `ATP_FLATTERM_DIFF`             | per-call flatterm-vs-tree mismatch counter (test-only)          |

The AtpFt flags stack — `*_NORM` requires `*_MATCH`, etc.; the
Makefile test targets bundle the correct subset.

### Public C setters

Apply at any time after `thvm_atp_init`.  Each setter mutates one
`AtpState` field; all changes survive the next `thvm_atp_step`.

Selection + queue ordering:

* `thvm_atp_set_cp_weight_mode(s, mode)` — `ATP_CP_WEIGHT_*` from
  ADD / MAX / ORD / GT / MIX / MIX2 / UNIF / GOAL / TWEE /
  LEARNED / CONJSYM / DIVERSITY / RELLEVEL / STAGGERED.
* `thvm_atp_set_selection_ratio(s, modulo)` — one FIFO pick every
  `modulo` weight picks (default 0 = pure weight).  WM's typical
  value is 51.
* `thvm_atp_set_random_modulo(s, modulo)` and
  `thvm_atp_set_random_seed(s, seed)` — Vampire-style random pop
  every Nth pick.
* `thvm_atp_set_goal_interleave(s, ratio)` — pick the most goal-
  relevant CP every Nth time.
* `thvm_atp_set_w2(s, modulo, mode)` — Waldmeister K-D Heap
  secondary dimension: every `modulo` picks, take the min of a
  second weight (the `mode` ATP_CP_WEIGHT_*).
* `thvm_atp_set_max_cp_weight(s, w)` and
  `thvm_atp_set_auto_max_cp_weight(s, base)` — hard / soft cap on
  CP weight; heavier CPs are dropped or stashed.
* `thvm_atp_set_cp_fifo_tiebreak(s, on)` — break heap-key ties by
  insertion order.

Filters:

* `thvm_atp_set_use_perm_subsume(s, on)` and
  `thvm_atp_set_perm_subsume_mask(mask)` — WM `GZ_ACVerzichtbar`
  filter: drop CPs whose two sides are AC-equal at the top symbol,
  for the bit-masked symbols.
* `thvm_atp_set_ac_mask(mask)` / `thvm_atp_get_ac_mask()` /
  `thvm_atp_auto_ac(lhs, rhs, n)` — register the engine-global AC
  bit-mask (each bit = "this CTR label is associative + commutative").
  When set, `atp_cp_trivially_joinable` treats AC-equal normal forms
  as joined.  `auto_ac` derives the mask from a caller-supplied
  axiom set.  Requires `-DTHVM_ATP_AC` at build time.
* `thvm_atp_set_use_rule_subsume_drop(s, on)` — WM `dokgS`:
  push-time rule-subsumption drop.
* `thvm_atp_set_use_fwd_subsume(s, on)` and
  `thvm_atp_set_use_bwd_subsume(s, on)` — Vampire-style forward
  and backward subsumption on rule add.
* `thvm_atp_set_use_bwd_demod(s, on)` — backward demodulation
  (rule's RHS rewriting existing rules).
* `thvm_atp_set_use_orphan_murder(s, on)` — drop queued CPs whose
  parent rules have retired.
* `thvm_atp_set_use_ground_join(s, on)` — Martin-Nipkow / Twee
  ground-joinability redundancy.
* `thvm_atp_set_use_connectedness(s, on)` — Bachmair-Dershowitz
  connectedness-below-peak redundancy.

Normalization paths:

* `thvm_atp_set_use_flatterm(s, on)` — flatterm-mixed normalize
  fast path (keeps the subject in flat arrays across orientable +
  unorientable inner loops).
* `thvm_atp_set_use_cp_index(s, on)` — opt-in CP-subterm
  discrimination index for CP enumeration.
* `thvm_atp_set_use_unorient_index(s, on)` — opt-in unorient-
  equation index for the ordered rewrite path.
* `thvm_atp_set_use_lazy_normalize(s, on)` — WM `lohntSichBehandlung`
  gate (push-time normalize only for small CPs).
* `thvm_atp_set_use_mnf(s, on)` — opt-in mandatory normal form
  (eager normalization with goal-directed front search).
* `thvm_atp_set_use_sos(s, on)` — Set-of-Support style restriction.
* `thvm_atp_set_use_lrs(s, on)` — Riazanov-Voronkov Limited
  Resource Strategy: prune the queue by predicted reachability.
* `thvm_atp_set_use_unfailing(s, on)` — toggle unfailing
  completion (default on; off = plain KB, unorientable rules
  reject the run).
* `thvm_atp_set_right_reduce(s, on)` — RHS-rewriting during
  interreduction.
* `thvm_atp_set_cp_set_interreduce(s, on)` — periodic full-rule
  re-normalization of the CP queue.
* `thvm_atp_set_record_norm_steps(s, on)` — emit per-rewrite-step
  trace records for proof reconstruction.

Ordering and goal:

* `thvm_atp_set_lpo(s, lpo_cfg)` — switch from KBO to LPO with the
  given precedence.
* `thvm_atp_set_spec(s, spec)` — sorted-signature spec (per-arg
  sorts; rejects ill-sorted inputs).
* `thvm_atp_set_goal_existential(s, on)` — treat the goal as
  `∃x. lhs(x) ≈ rhs(x)`; on proof, `s->witness_subst` carries
  the witness.

Wall budget:

* `thvm_atp_set_wall_deadline(s, seconds_from_now)` — soft wall
  cap; the saturator polls and returns `ATP_BUDGET` past the
  deadline.

ENIGMA-style learning hook:

* `thvm_atp_set_record_cp_features(s, on)` — emit per-CP feature
  vectors at push time for offline training.

### Env knobs

The bench harness (`tests/test_atp_wolfram_bench.c`) reads ~35 env
variables and translates them into the setters above.  These are
useful for shell-driven A/B benchmarking; programmatic callers
should prefer the setters directly.

| Env                                  | Translates to                                |
|--------------------------------------|----------------------------------------------|
| `ATP_BENCH_ORD=kbo\|lpo`              | choice of ordering for the bench engine     |
| `ATP_BENCH_LPO_SKOLEMS_HIGH=1`       | LPO precedence puts skolem consts highest    |
| `ATP_BENCH_GC=1`                     | force a GC every N steps                     |
| `THVM_ATP_CP_WEIGHT=<id>`            | `set_cp_weight_mode`                          |
| `THVM_ATP_SEL_RATIO=<N>`             | `set_selection_ratio`                         |
| `THVM_ATP_GOAL_INTERLEAVE=<N>`       | `set_goal_interleave`                         |
| `THVM_ATP_W2_MODULO`, `_MODE`        | `set_w2`                                      |
| `THVM_ATP_RANDOM_RATIO`, `_SEED`     | `set_random_modulo` + `_seed`                 |
| `THVM_ATP_PERM_SUB=1`                | `set_use_perm_subsume`                        |
| `THVM_ATP_PERM_SUB_MASK=<bitmask>`   | `set_perm_subsume_mask`                       |
| `THVM_ATP_RULE_SUB_DROP=1`           | `set_use_rule_subsume_drop`                   |
| `THVM_ATP_FWD_SUB=1`, `_BWD_SUB=1`   | `set_use_fwd_subsume` / `_bwd_subsume`        |
| `THVM_ATP_BWD_DEMOD=1`               | `set_use_bwd_demod`                           |
| `THVM_ATP_ORPHAN=1`                  | `set_use_orphan_murder`                       |
| `THVM_ATP_GROUND_JOIN=1`             | `set_use_ground_join`                         |
| `THVM_ATP_CONNECT=1`                 | `set_use_connectedness`                       |
| `THVM_ATP_RIGHT_REDUCE`              | `set_right_reduce`                            |
| `THVM_ATP_CP_SET_IR`, `_IR_PERIOD`   | `set_cp_set_interreduce` + the period field   |
| `THVM_ATP_RECORD_NORM=1`             | `set_record_norm_steps`                       |
| `THVM_ATP_LRS=1`                     | `set_use_lrs`                                 |
| `THVM_ATP_SOS=1`                     | `set_use_sos`                                 |
| `THVM_ATP_UNFAILING=0`               | disable unfailing completion                  |
| `THVM_ATP_MNF=1`                     | `set_use_mnf`                                 |
| `THVM_ATP_LAZY_NORM=1`               | `set_use_lazy_normalize`                      |
| `THVM_ATP_CP_FIFO=1`                 | `set_cp_fifo_tiebreak`                        |
| `THVM_ATP_FLATTERM=0`                | disable flatterm-mixed normalize              |
| `THVM_ATP_KBO_FLAT=0`                | disable KBO flatterm fast path                |
| `THVM_ATP_CP_INDEX=0`                | disable CP-subterm index                      |
| `THVM_ATP_UNORIDX=0`                 | disable unorient-equation index               |
| `THVM_ATP_UNORIENT_RESUME=0`         | disable unorient resume watermark             |
| `THVM_ATP_WMFPA=0`                   | disable WM-FPA flatterm fast path             |
| `THVM_ATP_SUBST_FLAT=1`              | use AtpFt-style direct subst splice           |
| `THVM_ATP_JOIN_CACHE=1`              | trivially-joinable verdict cache              |
| `THVM_ATPFT_NORM=1`                  | gate the AtpFt-norm path live                 |
| `THVM_ATPFT_RI=1`                    | gate the AtpFt-rule-index descent live        |
| `THVM_ATPFT_NORM_VERIFY=1`           | live differential against Term path           |
| `THVM_ATPFT_VERIFY=1`                | per-rule-add Term/AtpFt parity probe          |
| `THVM_ATP_LPO_ORIENT_CACHE=1`        | orient-cache live                             |
| `THVM_ATP_PROFILE=1`                 | print phase-time + counter breakdown at end   |
| `THVM_ATP_TRACE_MAX=<N>`             | cap on proof-trace length                     |
| `THVM_ATP_RULE_TRACE=1`              | print each rule as it's added (debug)         |
| `THVM_DEBUG_UNF_POS_MEMO=1`          | per-position memo verifier (debug)            |
| `THVM_ATP_DT_TRACE=1`                | discrim-tree descent trace (debug)            |
| `THVM_ATP_TICK_TRACE=<N>`            | print a status line every N steps             |
| `THVM_ATP_DIAG_DUMP=1`               | dump diagnostic state on UNSOUND aborts       |

The bench-level `THVM_ATP_WALDMEISTER=1` shorthand sets several of
these at once to match Waldmeister's typical run: SelectionRatio
141, CH_MixWeight, RHSInterreduce, UnfailingCP, AutoPrecedence on
the AtpFt-internal nand symbol.

## Soundness probes

### Live differential VERIFY

`THVM_ATPFT_NORM_VERIFY=1` makes every `atp_cp_trivially_joinable`
call run both the Term-path normalize and the AtpFt normalize,
then `ft_eq(ft_from_term(term_nf), atpft_nf)`.  Disagreement
aborts with a diagnostic.

### Per-rule-add verify

`THVM_ATPFT_VERIFY=1` runs `ft_eq(s->lhs_ft[k], ft_from_term(scratch,
s->lhs[k], 1)) && ft_hash(s->lhs_ft[k]) == atp_term_struct_hash(s->lhs[k])`
after every rule add.

### Per-position-memo verifier

`THVM_DEBUG_UNF_POS_MEMO=1` runs the descent on every memo-claimed
"no fire" position and aborts on disagreement.

### Build-time SELFCHECK

`-DATP_FLATTERM_SELFCHECK` compiles a build that runs both the
tree normalize and the flatterm-mixed normalize on every push-norm,
aborting on mismatch.  Build-only (defeats the speedup).
`-DATP_FLATTERM_DIFF` is the test-only counter variant — bumps
`g_atp_ft_diff_mism` on each mismatch so a test can `CHECK_EQ` it
to zero.

### GC

`thvm_atp_gc_collect` (`src/atp/_.c:4434`) is the engine's GC root
provider.  It gathers Term roots — `s->lhs[]`, `s->rhs[]`,
`s->r_dead_*_save[]`, `s->goal_*`, `s->trace[]`, witness bindings,
MNF colored nodes — into a Cheney evacuation
array, which the collector relocates.  Writeback stores the
relocated Terms back.  Term-keyed memos (`g_kbo_wmemo`,
`g_lpo_memo`) are invalidated; structural-hash memos
(`atp_unf_memo`, `atp_norm_cache`) survive unchanged because their
key isn't cell-keyed.

AtpFt cells are not relocated (slab-allocated, stable across the
run); GC doesn't touch them.

## Profiling

* **`THVM_ATP_PROFILE=1`** — emits per-phase wall-time accounting
  at bench exit (pop-norm, cp-gen, push-norm, interreduce,
  goal-check, norm-graph).  Phase sums can exceed wall because
  re-entrant inner loops are double-counted.

* **Counter dumps** — at PROFILE=1, additional counters print:

  * `lpo: top / rec / compute / memo-hit` — top-level call count,
    recursive depth, computed-not-memoed count, memo hit rate.
  * `unorient-step: calls / fires / empty / total` — the empty
    fraction is the cracker's main pain on saturating workloads.
  * `unf-memo` / `pos-memo` / `splice-inline` / `rhs-flat-cache` —
    each cache's hit ratio.
  * `ri-index` / `fv-index` — query count, tree nodes, candidates
    per query, `thvm_match` calls per query, prune ratio vs full
    rule list.

* **`sample` (macOS)** or **`dtrace`** — for true self-time
  attribution; the env-knob counters give phase-level breakdowns
  but not function-level self-time.

* **`THVM_ATP_RULE_TRACE=1`** — emit each rule as it's added to R
  (`atp_push_rule` log line).  Lets a script reconstruct the
  saturation trajectory for A/B comparison against wmcli or
  `FindEquationalProof`.

## TODOs

The AtpFt port is structurally complete; the open work-items live
in [roadmap.md](roadmap.md).  Quick pointers to the items that
touch this file's surface:

* **kbo_subtree_memo rekey.** Currently keyed by Term pointer; a
  shape-hash key would lift the 35% hit rate and let the LPO
  orient cache pay off.
* **Legacy mirror retire.** The Term-side `atp_dt_*` and
  `acp_unpack_term` paths still run alongside the FT paths.  Once
  every reader of `cp_packed[]` migrates to `cp_packed_ft[]`
  (FV-index), the byte queue can retire.
* **Unorient-step waste.** 96% of `atp_ft_unorient_step` calls on
  saturating workloads return no fire.  This is a CP-selection
  / search-strategy problem; addressing it is in roadmap.md.
