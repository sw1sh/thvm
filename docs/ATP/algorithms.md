# thvm/atp — Algorithms

This document describes the mathematical content the engine implements.
For the data structures that realize each operation, see
[engineering.md](engineering.md).

## Contents

* [Equational reasoning + Knuth-Bendix completion](#equational-reasoning--knuth-bendix-completion)
* [Reduction orderings: KBO and LPO](#reduction-orderings-kbo-and-lpo)
* [Term operations: matching, substitution, unification](#term-operations-matching-substitution-unification)
* [Critical pairs and superposition](#critical-pairs-and-superposition)
* [Normalization (rewriting to fixpoint)](#normalization-rewriting-to-fixpoint)
* [Interreduction](#interreduction)
* [Redundancy criteria](#redundancy-criteria)
* [Saturation loop](#saturation-loop)
* [Term indexing](#term-indexing)
* [Selection heuristics](#selection-heuristics)

## Equational reasoning + Knuth-Bendix completion

An equational theory is a set `E` of equations `l ≈ r`. The decision
problem **does `s ≈ t` hold in `E`?** is in general undecidable, but
becomes decidable when `E` can be converted into a *convergent term
rewrite system* — one that is both terminating (no infinite reduction
sequence) and confluent (any two reductions of the same term reach a
common form). In a convergent system the decision procedure is simply:
normalize both sides and compare.

Knuth-Bendix **completion** is the procedure for constructing such a
convergent system from `E`:

1. **Orient.** Pick a reduction ordering `>`. For each equation
   `l ≈ r`, if `l > r` orient as `l → r`, if `r > l` orient as
   `r → l`. Equations where neither side dominates are *unorientable*.
2. **Generate critical pairs.** When two rules `l₁ → r₁` and
   `l₂ → r₂` have an overlap (a position `p` in `l₁` such that
   `l₁|ₚ` unifies with `l₂` under MGU `σ`), the *critical pair*
   `⟨σ(l₁[r₂]ₚ), σ(r₁)⟩` may not be joinable in the current rule set.
3. **Add or close.** Normalize the CP's two sides. If they're equal,
   the CP is *joinable* (redundant). Otherwise add the new equation
   to `E` and loop back to step 1.

Saturation succeeds when every CP between every pair of rules
normalizes to a join — i.e. the rule set is closed under critical
pairs. Convergence then follows from Knuth-Bendix.

**Unorientable equations** are the wrinkle. Plain KB completion gives
up when an equation can't be oriented. *Unfailing completion* (Bachmair-
Dershowitz-Plaisted) handles these: keep the unorientable equation,
use it as a two-way conditional rewrite (apply the larger side at any
position where the ordering decides the instance), and superpose
both ways at CP-generation time. thvm implements unfailing completion;
this is why the rule set includes `r_orient[i]` flags and an
`unorient_index` parallel discrimination tree.

## Reduction orderings: KBO and LPO

Both orderings live in `src/kbo/` and `src/lpo/`. They take two terms
and return `KBO_GT | KBO_LT | KBO_EQ | KBO_UN` (or `LPO_*`). The
ATP engine wraps both via `atp_compare(s, lhs, rhs)` in
`src/atp/_.c:5133`. `KBO_UN` (incomparable) is what marks an
equation as unorientable.

### Knuth-Bendix Ordering (KBO)

KBO is parameterized by a per-symbol weight function `w : Σ → ℕ` and a
total precedence `>` on `Σ`, plus a variable weight `w(x) ∈ ℕ`. Define
the weight of a term:

```
weight(x) = w(x)                              for a variable
weight(f(t₁..tₙ)) = w(f) + Σ weight(tᵢ)       for a function
```

Then `s >_kbo t` iff every variable occurs at least as often in `s` as
in `t` (the var-balance condition) AND either:

* `weight(s) > weight(t)`, or
* `weight(s) == weight(t)` and one of:
  * `s = f(s₁..sₘ)`, `t = g(t₁..tₙ)`, and `f > g`, or
  * `s = f(s₁..sₘ)`, `t = f(t₁..tₘ)`, and `(s₁..sₘ) >_kbo (t₁..tₘ)` lexicographically.

Implementation: `thvm_kbo` walks both terms with a "divergence" cursor
(`kbo_vortest` in `src/kbo/_.c:575`). Identical sibling pairs cancel
and skip; divergent pairs accumulate weight differences and per-
variable counts into a balance state (`kbo_lin_addto`). At the end
the var-balance and weight delta decide.

A per-subtree memo (`kbo_subtree_memo`, `src/kbo/_.c:242`) caches each
CTR subterm's `(weight, var-profile)` keyed by Term cell + epoch.

### Lexicographic Path Order (LPO)

LPO is parameterized by a precedence `>` on `Σ` alone (no weights).
Recursively: `s >_lpo t` iff:

1. `s` is a function `f(s₁..sₘ)` and **some** `sᵢ ≥_lpo t`, OR
2. `t = g(t₁..tₙ)` with `f > g` and `s >_lpo tⱼ` for every `tⱼ`, OR
3. `s = f(s₁..sₘ)`, `t = f(t₁..tₘ)`, and `(s₁..sₘ) >_lpo (t₁..tₘ)`
   lexicographically (with `s >_lpo tⱼ` for the remaining children).

Implementation: `thvm_lpo` in `src/lpo/_.c:636`. A pos-keyed memo
(`g_lpo_memo`, `src/lpo/_.c:172`) caches verdicts. A separate
flatterm dispatch (`flatrec_gate`) pre-flattens both terms into
shape arrays and walks pos-indexed instead of pointer-indexed.

### Automatic precedence

`src/atp/precedence.c` derives a precedence from the axioms. It
classifies each binary symbol by the equational shapes it appears in
(commutativity `f(x,y) ≈ f(y,x)`, associativity, idempotence,
identity-element, inverse, distributivity); see `atp_analyze_axioms`
at `precedence.c:200`. Symbols with shape weights are then ordered by
occurrence and structural depth.

The Sheffer/`nand` workload uses a manual `p > q > r > nand` precedence
(skolems-high) so the LPO orients `nand`-built terms toward simpler
forms.

## Term operations: matching, substitution, unification

### One-way matching

`thvm_match(pattern, subject, &subst)` in `src/rewrite/_.c:14`
returns 1 iff there's a substitution `σ` such that `σ(pattern) =
subject`. `RewriteSubst` is a dense array `bindings[REWRITE_MAX_VAR]`;
the first sighting of a variable binds it, subsequent sightings
check `kbo_eq(existing, current)`. Used in rewriting (find a redex)
and in CP filtering (subsumption).

### Substitution application

`thvm_subst_apply(template, &subst)` walks the template and replaces
each variable with its binding, reusing the original cell when no
child changed (hash-cons skip at `src/rewrite/_.c:88`). Allocates new
CTR cells in the IC heap as needed.

### Unification

`thvm_unify(s, t, &subst)` in `src/unify/_.c` returns the MGU. Occur-
check enforced (no `x ↦ f(x)` cycles). Used by superposition at every
position-pair in the two rules.

`thvm_unify_apply` and `thvm_rename_vars` (renaming the second rule
into fresh variables before unification) are the wrappers superposition
calls.

## Critical pairs and superposition

For two rules `l₁ → r₁` and `l₂ → r₂` (renamed to disjoint variables),
a *critical overlap* exists at a non-variable position `p` of `l₁`
if `l₁|ₚ` and `l₂` unify under MGU `σ`. The *critical pair* is:

```
CP = ⟨ σ(l₁[r₂]ₚ), σ(r₁) ⟩
```

This is what overlapping rule `l₂` into `l₁` at position `p` "could
have given". A complete saturation generates CPs for every
non-variable overlap between every rule pair (and between a rule and
itself).

Implementation: `src/cp/_.c` enumerates positions and calls
`thvm_critical_pairs_pair` per pair; the ATP-internal driver is
`atp_overlap_ij` in `src/atp/_.c:10803`. For unfailing completion the
enumeration runs in both directions for unorientable rules (both
`l₁→r₁` and `r₁→l₁` faces).

## Normalization (rewriting to fixpoint)

`thvm_rewrite_normalize(t, lhs[], rhs[], n_rules, step_cap)` applies
rules until no rule LHS matches any subterm (the *normal form*).

Two normalize strategies live in `src/atp/_.c`:

* **Ordered** (`atp_rewrite_normalize_ordered`): every rewrite step
  is order-gated. A rule `l→r` rewrites `s` to `s[σ(r)]ₚ` only if
  `σ(l) > σ(r)` under the ordering, i.e. the rewrite is strictly
  decreasing. This is the standard *ordered rewriting* used during
  saturation when unorientable equations are in play — the equation's
  two faces are applied conditionally.

* **Indexed** (`atp_rewrite_normalize_indexed`): when every rule in
  `R` is KBO-oriented (`n_unorient == 0`), every rewrite is
  decreasing, so the discrim tree's outermost-leftmost redex is
  safe. This skips the per-rewrite order check.

A flatterm-mixed path (`atp_rewrite_normalize_flatterm_mixed`)
flattens the subject into shape arrays once at entry and splices
in-place via `atp_ri_splice` (`src/atp/_.c:2640`), avoiding the
per-step tree rebuild for the orientable inner loop. The
unorientable pass (`atp_ft_unorient_step`) runs over the same flat
arrays. This was iter 133's lever — keep the subject in flat form
across both passes.

The AtpFt path (`atp_rewrite_normalize_ft` in `src/atp/ft_norm.c`)
takes this further: cells are linked-list flatterm and never get
re-flattened. See [engineering.md §AtpFt](engineering.md#atpft-port).

## Interreduction

After a rule is added (via `atp_push_rule`), the engine runs
*interreduction*: existing rules whose LHS the new rule reduces are
demoted (their LHS is no longer in normal form, so they're not
valid as rewrite rules); existing rules whose RHS the new rule
reduces are *right-reduced* in place (their RHS is replaced by its
normal form). Sites: `atp_interreduce` in `src/atp/_.c:8746` (LHS-
collapse compaction), `:8820` (RHS in-place rewrite), `:8888` and
`:8918` (bwd-demod compaction).

The CP-queue analog, *CP-set interreduction*
(`atp_cp_set_interreduce`, `src/atp/_.c:8316`), periodically
re-normalizes every queued CP against the current rule set. Joinable
CPs are dropped, simplified CPs are re-weighted. Without this pass
the queue accumulates stale CPs derived from now-retired rules
("orphans") that pollute the priority order.

## Redundancy criteria

A CP that's redundant — provably won't change the saturation outcome
— can be dropped. thvm implements several:

* **Trivial join** (`atp_cp_trivially_joinable`,
  `src/atp/_.c:9309`): normalize both sides; if equal, drop.
  Strictest soundness criterion (always applies).

* **Forward / rule subsumption** (`atp_cp_rule_subsumed`,
  `src/atp/_.c:9376`): if some existing rule's `(l, r)` two-way
  matches the CP's `(lhs, rhs)` under a single substitution, the CP
  is a substitution instance of that rule and joinable via one step.

* **Queue subsumption** (`atp_cp_queue_subsumed`): if a CP already
  in the queue is more general than this one (via the FV index),
  drop the new one.

* **Permutation subsumption** (`atp_cp_perm_subsumed`,
  `src/atp/_.c:8678`): WM's `GZ_ACVerzichtbar` — drop CPs whose
  two sides are AC-equal (commutativity-shaped) at the top symbol.
  Gated by a per-symbol bit-mask so it fires only on declared-AC
  symbols.

* **Ground joinability** (`atp_cp_ground_joinable`, KBO only):
  build an ordered ground partition of the CP's variables and
  check that every ground instance is joinable. Soundness-preserving
  redundancy from Martin-Nipkow/Twee.

* **Bachmair-Dershowitz connectedness**
  (`atp_cp_connected_below_peak`): BFS from the two normalized
  sides; if they meet via rewrites strictly below the overlap peak,
  the CP is connected and dropable.

## Saturation loop

The driver is `thvm_atp_step(s)` in `src/atp/_.c:7184`. One step:

1. **Pop a CP.** Selection picks the heap-min by priority (with
   periodic FIFO/random/goal-directed/K-D-secondary picks; see
   [Selection heuristics](#selection-heuristics) below).
2. **Pop-time normalize.** Run the CP's two sides through the
   current rule set. If they reach the same NF, the CP is joinable
   — return `ATP_RUNNING` and pop the next one.
3. **Orient.** Compare the NFs under the ordering. If
   `lhs > rhs`, orient as `lhs → rhs`; if `rhs > lhs`, swap and
   orient. Otherwise the equation is unorientable: store both sides
   with `r_orient[k] = 0` and increment `n_unorient`.
4. **Commit-as-rule.** Append to `s->lhs[], s->rhs[], r_orient[],
   r_trace[]`. Insert into the discrim tree(s). Run interreduction.
5. **Generate offspring.** Compute critical pairs between the new
   rule and every existing rule. Push the surviving offspring CPs.
6. **Goal check.** If the goal's two sides are now joinable, return
   `ATP_PROVED`.

Status enum (`AtpStatus` in `src/thvm.h:3171`):

* `ATP_RUNNING` — continue.
* `ATP_PROVED` — goal joins.
* `ATP_SATURATED` — queue empty, no proof; `E` is now a convergent
  rewrite system but the goal is not a consequence.
* `ATP_BUDGET` — step cap or wall deadline hit.

## Term indexing

Two indexes accelerate the hot loops:

* **Discrimination tree** over rule LHSs (`AtpRuleIndex`,
  `src/atp/_.c:2153+`). One node per shared symbol prefix; leaves
  list the matching rules. Two trees: `rule_index` (orientable rules,
  used during ordered rewriting) and `unorient_index` (faces of
  unorientable rules, used by `atp_ft_unorient_step`).

* **FV index** over queued CPs (`AtpFvIndex`, `src/atp/_.c:1500+`).
  Componentwise feature vector (symbol counts, depth, var counts)
  with a trie keyed on each component; trie-walk yields candidates
  for queue-subsumption checks.

Discrim tree descent: `atp_ri_descend` (orientable, picks the minimum
rule index hitting a leaf) and `atp_ri_descend_unorient` (collects
every face whose pattern matches). The subject is pre-flattened
into parallel arrays `flat[]/subsz[]/flatsym[]` once per call; the
descent walks via index arithmetic, not Term pointers.

## Selection heuristics

The CP queue is a min-heap on `cp_pri[i]`, computed at push by one of
several weight modes (`atp_cp_weight_base`, `src/atp/_.c:5072`):

* **ADD** — `|lhs| + |rhs|` (symbol count).
* **MAX** — `max(|lhs|, |rhs|)`. WM's `CH_MaxWeight`.
* **GT** — KBO weight of the greater side when oriented, sum
  otherwise. WM's `CH_GtWeight`.
* **MIX** — `(wl+wr)·g + g + (wl+wr)` where `g` is the GT weight.
  WM's `CH_MixWeight`; this is the default for the Waldmeister
  preset.
* **MIX2** — `g·10 + (wl+wr)`. WM's `CH_MixWeight2`.
* **UNIF** — unification measure scaled by total weight.
* **GOAL** — symbol-overlap with the conjecture (E-style
  goal-directed).
* **CONJSYM**, **DIVERSITY**, **RELLEVEL**, **STAGGERED** — various
  E-style relevance heuristics.

Mixed in via periodic alternations:

* `selection_ratio` — every Nth pick is a FIFO pop (the oldest CP).
* `random_modulo` — every Nth pick is a Vampire-style random pop.
* `use_goal_interleave` — every Nth pick is the most goal-relevant
  CP.
* `w2_modulo` + `w2_mode` — every Nth pick is the heap-min of a
  *second* weight (Waldmeister's K-D Heap secondary dimension).

The goal-relevance score `cp_goal[i]` is the symbol overlap between
the CP and the conjecture (`atp_collect_symbols` for the conjecture
mask; per-CP overlap at push time).
