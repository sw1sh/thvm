# thvm/atp — Engineering

How the algorithms in [algorithms.md](algorithms.md) map to thvm's
data structures, what's hot, what's gated, and what guarantees soundness
across the optional paths. For the math, read algorithms.md first.

## Contents

* [Term representation](#term-representation)
* [AtpState](#atpstate)
* [Rule storage](#rule-storage)
* [CP queue](#cp-queue)
* [Discrimination tree](#discrimination-tree)
* [Memos and caches](#memos-and-caches)
* [AtpFt port](#atpft-port)
* [Env gates and feature stacking](#env-gates-and-feature-stacking)
* [Soundness probes](#soundness-probes)
* [Profiling and instrumentation](#profiling-and-instrumentation)

## Term representation

The engine operates on heap-cell `Term`s — thvm's universal 64-bit
packed pointer into a copying IC heap (shared with the rest of
thvm — IC reduction, UOP, WL bridge). A `Term` is one of `TAG_CTR`
(constructor with label + arity + child cells), `TAG_FVR` (free
variable with a 32-bit id), `TAG_NUM` (numeric leaf), plus a few
others that don't appear in ATP-internal terms. Accessors:
`term_tag(t)`, `term_ext(t)` (label / var id / dtype), `term_val(t)`
(child base or numeric value), `term_ctr_n(t)` (arity),
`term_ctr_at(t, i)` (child).

Heap cells are GC-relocated by a Cheney collector; the ATP engine
participates as a root provider (see [Soundness probes
§GC](#gc)) and invalidates its term-keyed memos on every GC.

### Why an alternative representation exists

The engine's hot loops (rule LHS matching, KBO/LPO walks, in-place
rewrite splicing) re-flatten the same Term subtrees many times per
saturation step. Each flatten walks heap cells via `term_ctr_at`,
which costs a heap read per node.

Waldmeister's `TermzellenT` is a different storage: each cell carries
`Nachf` (next-in-preorder) and `Ende` (end-of-subterm) pointers, so
the whole flat representation is *the storage* — no per-call
flatten. The ATP module ships a parallel `AtpFt` representation
mirroring this design (see [AtpFt port](#atpft-port)).

## AtpState

`AtpState` (in `src/thvm.h:3174..4125`) is the single per-engine
struct. The big fields:

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

Init: `thvm_atp_init` mallocs, calls `atp_ensure_rule_cap` for the
initial slot capacity, optionally `ft_init(&s->ft_arena)` under the
gates. Destroy: `thvm_atp_free` frees the arrays + the AtpFt arena
slabs.

## Rule storage

Append-only. `atp_push_rule(s, lhs, rhs)` (`src/atp/_.c:6723`)
writes `s->lhs[n_rules] = lhs; s->rhs[n_rules] = rhs;
r_orient[n_rules] = (atp_compare(s, lhs, rhs) == KBO_GT)`. If
unorientable, increment `n_unorient`. Marks the rule index dirty so
the next descent rebuilds.

Retirement happens during interreduction (`src/atp/_.c:6452`,
`:8746`, `:8888`, `:8918`). Two flavors:

* **LHS-collapse** — the new rule rewrites this rule's LHS, so this
  rule is no longer in normal form. Save the LHS/RHS into the
  `r_dead_*_save` slots (for proof reconstruction), shift the live
  arrays down, decrement `n_rules`.
* **Right-reduce** — the new rule rewrites this rule's RHS in
  place. Update `s->rhs[i]` to the reduced form; the rule stays.

The dead-save arrays preserve the original equation so the trace can
re-derive the retired rule's contribution.

### Rule index dirtying

Any rule storage mutation sets `s->rule_index_dirty = 1` and
`s->wmfpa_dirty = 1`. The discrim tree rebuilds at the next
`atp_unorient_step_indexed` or `atp_rewrite_normalize_indexed` call.
Three sites used to do this WITHOUT also calling
`atp_unf_memo_invalidate()`; that bug (fixed at commit 6be88f5f)
let stale "no-fire" verdicts outlive an R change. All three rebuild
sites now invalidate.

## CP queue

The queue is a min-heap on `cp_pri[]` with `cp_seq[]` as a stable
FIFO tiebreaker. Each slot `i` stores:

* `cp_packed[i]` — pointer to a malloc'd byte string that encodes
  the CP's two terms in pre-order varint format (`acp_pack` in
  `src/atp/_.c:864`). 1 discriminator byte + LEB128 varints per
  symbol. Self-delimiting (the unpacker walks pre-order via
  discriminator + arity until two terms are reconstituted).
* `cp_pri[i]`, `cp_seq[i]`, `cp_goal[i]`, `cp_pri2[i]` — the four
  parallel heap-key arrays.
* `cp_trace[i]` — trace index for the CP's two parent rules.

The byte-string encoding is small (avg ~12 B per CP) and survives GC
unchanged because it contains no Term cell pointers — just symbol
labels and var ids. Heap insertion is the standard sift-up/sift-down
in `atp_cp_swap` + `atp_cp_sift_*`.

Pop (`thvm_atp_select_cp`, `src/atp/_.c:6304`) decodes via
`acp_unpack` into two fresh heap Terms. Periodic non-min picks
(FIFO/random/goal/K-D) replace the min-pop with a linear scan over
the queue.

### CP-graph mirror

An optional `cp_graph` Term-DAG mirror (`#ifdef ATP_CP_GRAPH`,
`src/atp/_.c:952..1026`) re-decodes every queued CP into a Term
node and bundles them into a `CpSet[...]` CTR. Used by some
classifier / learning hooks. Re-built after every queue mutation
via `atp_cp_graph_sync`.

### AtpFt CP queue mirror

Under `THVM_ATPFT_CPQ`, a parallel `AtpCpEntry[]` array holds the
same CPs as native AtpFt cell-pairs (24 B header + cells in Arena
A). Decode-free pop; trivially-joinable check operates on
`AtpFtCell *` directly. See [AtpFt §CP queue
mirror](#atpft-cp-queue-mirror).

## Discrimination tree

`AtpRuleIndex` (`src/atp/_.c:2153..2698`) is a left-child / right-
sibling tree with a single `sym : u32` edge label per node, drawn
from three disjoint ranges:

* `[CTR_BASE, CTR_BASE + n_labels)` — exact constructor label.
* `NUM` — any numeric leaf (collapsed to one symbol).
* `[STAR_BASE, STAR_BASE + ATP_RI_MAXVARS)` — a rule variable, with
  the *first-appearance* var-renumbering. Distinct rule vars get
  distinct STAR edges, so the tree distinguishes `f(x,x)` from
  `f(x,y)` structurally.

`atp_ri_rebuild` (`src/atp/_.c:2368`) walks every live rule LHS in
pre-order and inserts. Leaves carry an `AtpRiRec` list pointing back
into `s->lhs[]`.

Descent (`atp_ri_descend` for orientable, `atp_ri_descend_unorient`
for unorientable) walks the subject's pre-flattened arrays
position-by-position. STAR edges bind a subject preorder position
into `g_atp_ri_star[k]`; repeat-var consistency is a `memcmp` over the
bound slice's flatsym. Backtrack is implicit (recursive descent's
call frames undo the bind). At a leaf, if `perfect = !ix->any_folded
&& !g_atp_ri_query_folded` then arriving at the leaf IS the match
proof; otherwise the descent falls through to a `thvm_match` against
the stored pattern.

**Var folding.** `ATP_RI_MAXVARS = 64` is the per-rule var-slot
budget. A rule with more distinct vars gets its high-id vars
"folded" onto the last slot, making the tree coarser; the leaf then
re-runs `thvm_match` as the authoritative guard.

### FV index

`AtpFvIndex` (`src/atp/_.c:1500..1738`) keys on a per-CP
feature vector — symbol-multiset, depth, var count, etc. — and
returns CPs whose FVs are componentwise-dominated by the query. Used
by `atp_cp_queue_subsumed` to find CPs the new one might be a
substitution instance of. Cheaper than walking the whole queue.

## Memos and caches

The engine carries several memos, each with its own invalidation
discipline:

| Memo                       | Key                          | Invalidation                    |
|----------------------------|------------------------------|---------------------------------|
| `g_kbo_wmemo`              | (Term, epoch)                | `thvm_kbo_invalidate()` (GC)    |
| `g_lpo_memo`               | (Term, Term, epoch)          | `thvm_lpo_invalidate()` (GC)    |
| `g_atp_unf_memo`           | atp_term_struct_hash(t)      | every rule_index rebuild        |
| `g_atp_unf_pos_memo`       | (subtree_phash, folded bit)  | every rule_index rebuild        |
| `g_atp_norm_cache`         | atp_term_struct_hash(t) + R-epoch | on R change                |
| `g_atp_orient` (opt-in)    | (lhs_hash, rhs_hash, epoch)  | on KBO/LPO precedence change    |

All are direct-mapped, epoch-stamped (so invalidation is `epoch++`,
O(1) until wrap). Soundness depends on the invalidation hook firing
on every R-change for memos keyed on R-dependent verdicts. The
`atp_unf_memo` family bumps its epoch from inside
`atp_ri_rebuild`'s prologue — at three rebuild sites
(`unorient_step_indexed`, `rewrite_normalize_indexed`,
`rewrite_normalize_flatterm_mixed`).

### Why memos don't make caching free

Stage 8's LPO orientability cache added a top-level
`(hash(lhs), hash(rhs)) -> verdict` lookup before `thvm_kbo`. On the
AndAssoc workload the cache hit rate is 99.4% on the standalone
test but the live bench is ~1% slower than baseline. Reason: the
existing `g_kbo_wmemo` already catches the redundancy at a lower
level; the new top-level cache's hash+probe overhead exceeds the
saved compute on the residual cache-missable calls.

This is the general pattern across the engine: the memos at the
bottom of each hot loop already deduplicate. New caches help only
when they catch redundancy the existing memos miss, AND when their
own overhead is below the saved compute.

## AtpFt port

`src/atp/ft*.c` is a parallel native-flatterm representation modeled
on Waldmeister's `TermzellenT`. Each cell is 24 B (8-byte aligned),
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

`cell->end->next` gives the next sibling. `cell->sym & 0x80000000`
flags variables.

### Arenas

Two arenas (`src/atp/ft_alloc.c`):

* **Persistent (Arena A)** — 30000-cell slab pool, free-list per
  slab. Cells are stable across allocations (slabs malloc'd once
  and never realloc'd). Holds anything reachable from a root:
  `s->lhs_ft[i]`, `s->rhs_ft[i]`, AtpFt-mirror CP queue entries.
  `ft_alloc_persistent` pops the free list (refills with a fresh
  slab on empty); `ft_free_span(first, last)` is O(1) chain-push
  (mirrors WM's `TermzellenlisteLoeschen`).

* **Scratch (Arena B)** — bump-pointer over a single contiguous
  region (256 KB initial, realloc 2x on overflow). `ft_scratch_reset`
  is one `top := base` assignment. Used for per-call subject flatten
  in the boundary converter.

### Operations

The AtpFt module exports:

* **Constructors** (`src/atp/ft.c`): `ftnew_var`, `ftnew_const`,
  `ftnew_ctr`. Stitch `next`/`end` pointers, OR-fold the GROUND flag.
* **Converters**: `ft_from_term` (Term -> AtpFt), `ft_to_term`
  (AtpFt -> Term). O(|term|).
* **Equality + hash**: `ft_eq` (structural), `ft_hash` (byte-identical
  FNV-64 recurrence with `atp_term_struct_hash`, so a Term and its
  ft_from_term image hash to the same u64).
* **Pretty-print**: `atp_pretty_ft` (byte-identical output to
  `atp_pretty_term`).
* **Ordering** (`src/atp/ft_order.c`): `thvm_kbo_ft`, `thvm_lpo_ft`
  flatten cells into `KboFlatNode[]` once per call and dispatch to
  the existing `thvm_kbo_flat_slice` / `lpo_flat_rec_compute`.
* **Match + subst-apply** (`src/atp/ft_match.c`): `ft_match` (pattern
  vs subject -> `AtpFtSubst` slot table with watermark backtrack).
  `ft_subst_apply` builds the instantiated template into Arena B.
* **Splice** (`src/atp/ft_splice.c`): WM `SigmaRInEZ`/`InLZ`-style
  in-place rewrite at a redex position. Marks every freshly emitted
  cell with `ATPFT_FLAG_SUBST_FRESH`; the next normalize-fixpoint
  pass skips those cells (WM's `BL_NormalformInnermost2`
  innermost-skip trick).
* **Normalize** (`src/atp/ft_norm.c`): `atp_rewrite_normalize_ft`
  fixpoint. Entry-clear of all `SUBST_FRESH` bits; loop redex-find
  + splice; clear at exit.
* **Discrim-tree descent** (`src/atp/ft_ri.c`): `atp_ri_descend_ft_at`
  is the AtpFt-pointer-walking parallel to `atp_ri_descend_rec`. The
  tree itself is shared with the Term side (symbol-skeleton encoding
  is representation-agnostic); only the terminal `ft_eq` consistency
  check differs.

### CP-queue mirror

`src/atp/ft_cpq.c` adds an `AtpCpEntry { lhs, rhs; size; weight;
priority; origin_rule; }` array parallel to `cp_packed[]`. Push,
pop, swap, move, compact all mirror the byte-queue. The native
joinability check `atp_cp_trivially_joinable_ft` runs
`atp_rewrite_normalize_ft` on both sides + `ft_eq` for the verdict —
no `ft_from_term` at entry, no `ft_to_term` at exit.

### Why AtpFt didn't move the bench

Stages 6, 7, 6b all landed sound at parity. The reason
(post-profile analysis):

* Per-CP cost wasn't the bottleneck. The existing flatterm-mixed
  path (Stage 0, pre-AtpFt) already keeps the subject in flat form
  across the orientable+unorientable inner loops.
* The dominant cost is `atp_ft_unorient_step`: 2.74M calls per 20s
  of AndAssoc, 96% of them returning "no fire". Each empty call
  still pays a flatten + pos-memo lookup + occasional descent. The
  AtpFt path doesn't change this control flow.
* The rule-acquisition rate decays 20x as |R| grows from 397 to
  640 because each rewrite gets more expensive (more rules to
  test, more unorientable rules to order-gate). This is the
  trajectory problem, not the per-CP problem.

The AtpFt port is structurally complete, sound across every flag
combination including live differential `THVM_ATPFT_NORM_VERIFY`,
and gives the engine a representation that *could* close
specific cost lines (e.g. WM's `subst_fresh` innermost skip works
correctly under the AtpFt path) but doesn't reach the
algorithmic bottleneck identified by the profile.

## Env gates and feature stacking

| Env flag                          | What it enables                                  | Requires             |
|-----------------------------------|--------------------------------------------------|----------------------|
| `THVM_ATPFT_ALLOC`                | AtpFt arena allocator + ft.h cell type           | -                    |
| `THVM_ATPFT_CONVERT`              | ftnew_*, ft_from/to_term, ft_hash, ft_eq         | ALLOC                |
| `THVM_ATPFT_LPO`                  | thvm_kbo_ft + thvm_lpo_ft                        | CONVERT              |
| `THVM_ATPFT_MATCH`                | ft_match + ft_subst_apply                        | LPO                  |
| `THVM_ATPFT_RULES`                | s->lhs_ft / rhs_ft dual-store                    | CONVERT + LPO        |
| `THVM_ATPFT_NORM`                 | atp_rewrite_normalize_ft + ft_splice in join     | RULES + MATCH        |
| `THVM_ATPFT_RI`                   | atp_ri_descend_ft (cell-walking discrim tree)    | NORM                 |
| `THVM_ATPFT_CPQ`                  | native AtpFt CP queue alongside byte queue       | RI                   |
| `THVM_ATPFT_NORM_VERIFY`          | live differential: run BOTH paths, abort on mismatch | NORM             |
| `THVM_ATPFT_VERIFY`               | per-rule-add scratch round-trip parity check     | RULES                |
| `THVM_ATP_LPO_ORIENT_CACHE`       | LPO/KBO orientability cache (top-level)          | -                    |

Each test binary in `tests/test_atp_ft*.c` is `tests/test_atp.c`
compiled with a specific subset of the flags. The strongest
acceptance gate is `THVM_ATPFT_NORM_VERIFY=1` combined with the
full AtpFt stack — every CP normalize runs through both the Term
path and the AtpFt path and aborts on any disagreement. All 135624
ATP assertions pass under that mode.

Other ATP-specific env flags (the non-AtpFt ones) are documented
near their handlers in `tests/test_atp_wolfram_bench.c` (the bench
harness) and live in `AtpState` mode fields with public setters
like `thvm_atp_set_use_mnf`.

## Soundness probes

### Live differential VERIFY

`THVM_ATPFT_NORM_VERIFY=1` makes every `atp_cp_trivially_joinable`
call run both the Term-path normalize and the AtpFt normalize, then
`ft_eq(ft_from_term(term_nf), atpft_nf)`. Disagreement aborts with a
diagnostic. The full 135624-assertion `test_atp` corpus passes
under this gate combined with the full AtpFt stack.

### Per-rule-add verify

`THVM_ATPFT_VERIFY=1` runs `ft_eq(s->lhs_ft[k], ft_from_term(scratch,
s->lhs[k], 1)) && ft_hash(s->lhs_ft[k]) == atp_term_struct_hash(s->lhs[k])`
after every rule add. Catches drift between the Term and AtpFt
storage at write time.

### Per-position-memo verifier

`THVM_DEBUG_UNF_POS_MEMO=1` runs the descent on every memo-claimed
"no fire" position and aborts on disagreement. Caught the
`unf_memo_invalidate` rebuild-site bug at iter 195 (commit
6be88f5f).

### Build-time SELFCHECK

`-DATP_FLATTERM_SELFCHECK` compiles a build that runs both the tree
normalize and the flatterm-mixed normalize on every push-norm,
aborting on mismatch. Build-only (defeats the speedup).
`-DATP_FLATTERM_DIFF` is the test-only counter variant — bumps
`g_atp_ft_diff_mism` on each mismatch so a test can `CHECK_EQ` it
to zero.

### GC

`thvm_atp_gc_collect` (`src/atp/_.c:4434`) is the engine's GC root
provider. It gathers Term roots — `s->lhs[]`, `s->rhs[]`,
`s->r_dead_*_save[]`, `s->goal_*`, `s->trace[]`, witness bindings,
CP-graph mirror, MNF colored nodes — into a Cheney evacuation array,
which the collector relocates. Writeback stores the relocated Terms
back. Term-keyed memos (`g_kbo_wmemo`, `g_lpo_memo`) are invalidated;
structural-hash memos (`atp_unf_memo`, `atp_norm_cache`) survive
unchanged because their key isn't cell-keyed.

AtpFt cells are not relocated (slab-allocated, stable across the
run); GC doesn't touch them.

## Profiling and instrumentation

* **`THVM_ATP_PROFILE=1`** — emit per-phase wall-time accounting at
  bench exit (pop-norm, cp-gen, push-norm, interreduce, goal-check,
  norm-graph). Phase sums can exceed wall because re-entrant inner
  loops are double-counted.

* **Counter dumps** — at PROFILE=1, additional counters print:

  * `lpo: top / rec / compute / memo-hit` — top-level call count,
    recursive depth, computed-not-memoed count, memo hit rate.
  * `unorient-step: calls / fires / empty / total` — the empty
    fraction is the cracker's main pain (96% on AndAssoc).
  * `unf-memo` / `pos-memo` / `splice-inline` / `rhs-flat-cache` —
    each cache's hit ratio.
  * `ri-index` / `fv-index` — query count, tree nodes, candidates
    per query, thvm_match calls per query, prune ratio vs full
    rule list.
  * `splice-inline hits/misses` (AtpFt) — how often the in-place
    splice path took over from the deep-copy fallback.

* **`sample` (macOS)** or **`dtrace`** — for true self-time
  attribution. The Stage-7 profile that identified the real
  bottlenecks used `sample` at 1ms over a 22s AndAssoc run; results
  in commits `af4b3ae9` and earlier scoreboards.

* **`THVM_ATP_RULE_TRACE=1`** — emit each rule as it's added to R
  (`atp_push_rule` log line). Lets a script reconstruct the
  saturation trajectory for A/B comparison against wmcli or
  FindEquationalProof.
