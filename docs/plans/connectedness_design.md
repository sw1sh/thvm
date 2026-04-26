# Connectedness redundancy: design memo (stage 7.2a)

> Sibling of `waldmeister_ic_atp.md` and `saturation_loop.md`.
> Decision document for stage 7.2's concrete implementation.

## Problem statement

Stage 7.1 (committed `e7913b9`) added a *trivial-joinability filter*:
when a critical pair `(s, t)` is generated, normalize both sides under
the current rule set R via `thvm_rewrite_normalize` and drop the CP if
the normal forms are equal. The normalize function is recursive (top
+ left-to-right descent into CTR children, see
`src/rewrite/_.c:102-124`), so sub-position redexes are reduced as
well as top redexes.

Stage 7.2 calls for *connectedness redundancy* in the
**Bachmair-Dershowitz-Plaisted** sense ("Completion without Failure",
RTA 1989). The literal BDP definition: a CP `(s, t)` is *connected
below `c`* if there exist intermediate terms
`s = u_0, u_1, ..., u_n = t` such that for each `i`,
`u_i ↔_R u_{i+1}` and every `u_i` is strictly smaller than `c` in the
reduction ordering. The classical use is `c = max_>_(s, t)`: a CP is
BDP-connected if both sides can be joined via rewrites that all stay
below the larger side.

The design question: **is BDP connectedness as a standalone filter
worth implementing in our codebase, given that 7.1's trivial-
joinability already does full normalization under R?**

## Three candidate criteria

| Criterion | Formal statement | Pruning relative to 7.1 | Cost |
|---|---|---|---|
| **(1) Subsumption-connected** | `(s, t)` is connected if there is a single rewrite step under R that takes `s -> t` (or `t -> s`) | Strict subset of 7.1 (1-step is subsumed by full normalize) | O(\|R\| * \|s\|) |
| **(2) Source-rule-disjoint connected** | `(s, t)` from rules `R_a, R_b` is connected if it is joinable under `R \ {R_a, R_b}` | Strict subset of 7.1 (smaller rule set cannot reach more joins) | 2 * normalize, with rule masking |
| **(3) BDP "below `c`"** | `(s, t)` is connected if joinable under `R_<c` where `R_<c = {l -> r ∈ R : max(l, r) <_KBO c}` and `c = max_KBO(s, t)` | Strict subset of 7.1 (using fewer rules to reach the same NFs) | normalize with KBO-filtered rule set |

## Domination argument

For all three candidate criteria, *the rule subset used for the
joinability check is a subset of R* (the full rule set used by 7.1).
The fundamental observation:

> **Lemma**: If `(s, t)` is joinable under `R' ⊆ R`, then it is
> joinable under `R`.
>
> **Proof**: Any rewrite path that uses rules from `R'` is itself a
> rewrite path under `R`, since `R' ⊆ R`. The same join sequence
> works.

Therefore, every CP that any of (1), (2), (3) drops is also dropped
by 7.1. None of these criteria adds new pruning power on top of 7.1.

## Worked examples

**Example A** (no help from any criterion).
`R = {a -> b, a -> c}`. Top-overlap of rules 0 and 1: CP = `(b, c)`.
- 7.1: NF(b) = b, NF(c) = c. NOT joinable. CP survives.
- (2) under `R \ {0, 1}` = {}: NF(b) = b, NF(c) = c. NOT joinable.
- (3) under `R_<max(b,c)` = {} (no rule has both sides smaller than
  b or c): same.

CP genuinely survives in all cases -- correct.

**Example B** (7.1 fires, (2) and (3) also fire trivially).
`R = {f(x) -> a, f(x) -> b, g(a) -> c, g(b) -> c}`. Top-overlap of
rules 0 and 1: CP = `(a, b)`.
- 7.1: NF(a) = a, NF(b) = b. NOT joinable. Survives.

(All three criteria agree: not joinable, survives.)

**Example C** (looking for a case where (2) drops but 7.1 doesn't --
*there isn't one*).
By the lemma, if (2) drops then 7.1 drops. The contrapositive is
what we'd need to find a counterexample to, and the lemma forbids it.

## Why standalone BDP doesn't help our codebase

In the original BDP paper, connectedness is the *justification* for
discarding non-joinable CPs without losing completeness. In an
implementation that uses `thvm_rewrite_normalize` for joinability
testing, the joinability check is *already* the strongest possible
redundancy criterion derivable from rewriting: it succeeds iff a
join exists in the deterministic-strategy sense.

BDP connectedness adds value in implementations that *do not*
normalize aggressively (e.g., where joinability is tested only
modulo a special equational theory like AC). In our pure equational
implementation, it is theoretically interesting but practically
subsumed.

## Decision: implement criterion (2) as an empirical demonstration

We will implement source-rule-disjoint connectedness (criterion 2)
in stage 7.2b *not* because it adds new pruning, but because:

1. It documents the BDP redundancy structure in code, alongside
   `n_cps_dropped_joinable`.
2. It produces a measurable counter `n_cps_dropped_connected` that
   can be compared to `n_cps_dropped_joinable` empirically.
3. The expected outcome -- counter is bounded above by
   `n_cps_dropped_joinable` -- is an *empirical confirmation* of
   the domination lemma, useful for future architectural decisions
   (e.g., when AC matching lands in stage 7.4+, criterion (2) might
   start firing on cases 7.1 misses).
4. The implementation (rule-masking + normalize + compare) is small
   (~50 LOC) and the code is reusable for future criteria.

The honest framing in the CHANGELOG and tests should be:
"connectedness counter, expected to be a strict lower bound on
trivial-joinability counter; useful when AC theories or fancier
joinability checks are introduced."

## Implementation sketch (for stage 7.2b)

```c
// Stage 7.2b: source-rule-disjoint connectedness.  Returns 1 if
// (lhs, rhs) is joinable under R \ {rule_a, rule_b}.  By the
// domination lemma, this implies trivial-joinability under R, but
// the converse is not true; the counter is bounded above by 7.1's
// counter.  rule_a / rule_b are indices into s->lhs[] / s->rhs[];
// ATP_RULE_NONE means "no rule excluded."
static u8 atp_cp_source_disjoint_connected(
    AtpState *s, Term lhs, Term rhs, u32 rule_a, u32 rule_b);
```

Wired into `atp_push_cps_traced` *after* 7.1's filter (so the
counter only ticks for CPs that survived 7.1 -- but by the
domination lemma, it never ticks; we instead check it on a
copy-of-CP path that does NOT also run 7.1, purely for measurement).

Actually simpler: in stage 7.2b, the counter ticks unconditionally
when criterion (2) fires, *regardless* of whether 7.1 also fires.
This lets us empirically measure the overlap.

Cost per CP: 2 normalize calls with `n_rules - 2` rules + a small
filtering pass.

## Verification (for stage 7.2b)

Tests:
1. **Counter ticks on group example**: full saturation produces
   non-zero `n_cps_dropped_connected`.
2. **Domination empirically holds**:
   `n_cps_dropped_connected <= n_cps_dropped_joinable` on all our
   test inputs.
3. **Genuine CP survives**: hand-construct `R = {a -> b, a -> c}`,
   verify the CP `(b, c)` is not dropped by either criterion.
4. **Source-rule masking works**: hand-construct a case where the
   filtered rule set is empty and verify joinability falls through
   correctly.

## Stage 7.3 follow-on

Subsumption pruning (stage 7.3) is the orthogonal criterion: a CP
`(s, t)` is *subsumed* if there exists `(l, r) ∈ R` and substitution
`σ` such that `(s, t) = (σl, σr)` (modulo symmetry). Subsumption
fires on CPs where the underlying equation is already an instance
of an existing rule, regardless of joinability. This *can* fire on
CPs that 7.1 misses, since 7.1 only drops when both sides reduce
to the same NF -- it does not detect "this CP is just a substituted
instance of an existing rule."

Recommended ordering: implement 7.3 (subsumption) early; it provides
real pruning. 7.2b is best done as a small, well-documented
empirical exercise demonstrating the domination relationship.
