# Wolfram-axiom Boolean corpus: design memo (stage 10a)

> Sibling of `corpus_expansion_design.md` (9.4a).  Picks the two
> Wolfram-axiom conjectures for 10b to implement, with predicted
> status under our 32-step bench budget.

## Goal

Stage 9.4 closed the GRP / RNG / LCL / LAT corpus expansion with
`comm_monoid_swap` (QUEUE_EMPTY, AC-redundancy gap) and the two
`group_*` TIMEOUTs as the headline failures.  Stage 10 adds two
fixtures from a different corner: **Stephen Wolfram's 2000
single-equation axiomatisation of Boolean algebra**, proven
equivalent to standard Boolean algebra by McCune et al. via
EQP (J. Automated Reasoning 2002).

The Wolfram axiom in NAND form (sole binary symbol):

    ((x NAND y) NAND z) NAND (x NAND ((x NAND z) NAND x)) = z

This is a famous ATP benchmark precisely because *one*
equation is enough to derive every Boolean-algebra theorem,
but the lemmas needed to bootstrap derivations are deep.
McCune's original equivalence proof took hours of EQP
saturation; modern Twee/E handle it in seconds with strong
indexing.  Our 32-step bench budget puts non-trivial
conjectures firmly in TIMEOUT territory.

This memo picks one PROVED-fast conjecture (so the bench has
something the prover can close) and one TIMEOUT-stress
conjecture (the Sheffer-commutativity classic).

## Selected candidates

### 1. `wolfram_axiom_literal.pr` (CONSERVATIVE, predicted PROVED @ 0)

**Source.** Direct instantiation of the Wolfram axiom with
concrete constants.

**Signature.**

- `nand: ANY ANY -> ANY`   (sole binary operator)
- `p: -> ANY`              (constant)
- `q: -> ANY`
- `r: -> ANY`

**Axioms.**

- `nand(nand(nand(x, y), z),
        nand(x, nand(nand(x, z), x))) = z`        (Wolfram)

**Conjecture.** `nand(nand(nand(p, q), r),
                       nand(p, nand(nand(p, r), p))) = r`

**Predicted status.** PROVED @ 0.  The conjecture is a literal
instance of the axiom with `x=p, y=q, z=r`; the goal-rewrite
path applies the Wolfram rule once at the top and reduces
lhs to `r`.  RHS is already `r`.  Bench reports PROVED before
any saturation step fires.

**KBO vs LPO orientation.**  Precedence
`nand > p > q > r`.  The axiom's lhs has depth 4 (nested nands)
while rhs is just the variable `z`; under both KBO and LPO,
variables are strictly less than any function term whose head
has positive precedence weight, so `lhs > rhs`.  The axiom
orients cleanly in one direction.

**Why this pick.**  Sanity check that the parser, KBO/LPO
config, and goal-rewrite path all handle the Wolfram axiom's
unusual depth-4 nesting.  Failure here would point to a parse
or rewrite limit, not a redundancy-criteria gap.

### 2. `wolfram_sheffer_commutativity.pr` (STRESS, predicted TIMEOUT @ 32)

**Source.** Sheffer-NAND commutativity.  The classic first
derived theorem from the Wolfram axiom in McCune's original
proof; equivalent to "show that the Wolfram axiom implies
the symmetry of NAND".

**Signature.** Same as #1, plus `a, b: -> ANY` constants for
the conjecture.

**Axioms.** Same single Wolfram axiom as #1.

**Conjecture.** `nand(a, b) = nand(b, a)`.

**Predicted status.** TIMEOUT @ 32.  McCune's EQP took hours
on this; the lemmas needed include Boolean idempotence
(`nand(nand(x, x), nand(x, x)) = x`), absorption-like
identities, and several non-obvious associativity-style
rewrites.  Our prover at 32 steps will saturate ~20 rules and
hit the budget without producing the lemma chain.

Twee with `--max-cps 256` should still PROVE this (Twee proved
both `comm_monoid_swap` and `group_*` cases that we TIMEOUT on,
so its ground-joinability + connectedness criteria carry the
day).  9.4c's "thvm wins on easy / Twee wins on lemma
discovery" pattern repeats here.

**KBO vs LPO orientation.**  Same Wolfram axiom orientation;
the conjecture has FVRs only on rhs (`a` and `b` are constants
in the goal terms but the rewrite-and-compare path treats them
as ground), so the goal-check normalises both sides under the
single axiom rule and the result is whatever non-trivial
canonical form the saturation reaches.  Without commutativity-
discovery the two sides won't match.

**Why this pick.**  Stress-tests where lemma discovery is
genuinely hard.  Pairs naturally with the existing
`group_left_id_from_assoc` TIMEOUT entry to give 10c's bench
comparison two distinct hard problems.

## Aggregate prediction

| Fixture | Predicted | Step bound | Notes | Observed (10b) |
|---|---|---|---|---|
| `wolfram_axiom_literal`         | PROVED  | 0      | Literal axiom instance | PROVED @ 0    |
| `wolfram_sheffer_commutativity` | TIMEOUT | 32 cap | Lemma discovery        | DROPPED, see below |

**10b finding:** the Sheffer-commutativity stress fixture
crashes the bench harness under IC-rewrite modes.  Under cc
(C-direct cp-gen + C-direct rewrite) it ran TIMEOUT @ 32 in
47 ms with 32 rules and 1133 trace entries -- the predicted
behaviour.  Under ci (C-direct cp-gen + IC-routed rewrite)
the IC-rewrite path on the depth-4 NAND axiom exhausts the
16M-cell HEAP_CAP before the budget is reached.

The cell-budget per IC-rewrite step on the Wolfram axiom is
roughly 50-100 cells (depth-4 nesting reconstructed via
`term_new_ctr` at every recursive `atp_ic_rewrite_step`
call).  Saturation generates ~32 fresh rules x ~32 rewrite
steps each, with each step doing two normalize calls (lhs,
rhs) of NORM_CAP=64 iterations.  Worst case: 32 * 32 * 2 *
64 * 100 = 13 M cells -- right at the HEAP_CAP boundary.
9.3's heap checkpoint/reset only fires on the joined-CP
branch, which is rare on this fixture (most CPs are real).

Per this memo's stop condition ("If the Wolfram axiom's
nested-nand structure overflows the parser's term-depth
limits, document the bound and revisit"), 10b ships with
**only the literal fixture**.  The observed-but-uncommitted
data on the stress fixture (cc-mode TIMEOUT @ 32) is the
useful artefact; full bench-Twee comparison on it is
deferred until either:

- HEAP_CAP is bumped past 16M (pre-step heap usage profile
  needed first);
- 9.3's heap reset is widened beyond the joined-CP branch;
- a less-extreme stress conjecture (e.g., a partial Sheffer
  result, or a derived self-NAND identity) replaces the
  full commutativity goal.

Brings corpus from 12 to 13 fixtures (1 added, not 2).

## KBO-vs-LPO note

The 8.5d / 9.4c claim "KBO and LPO agree on our corpus"
should survive these picks: the Wolfram axiom orients
identically under both (lhs depth 4 vs rhs variable, no
weight-vs-precedence tradeoff that would flip the comparison).
9.4c documented zero KBO-vs-LPO disagreements; 10c will check
again.

## Stop conditions

If 10b shows `wolfram_axiom_literal` returns RUNNING / TIMEOUT
instead of PROVED, the Wolfram axiom's depth-4 lhs is hitting
some parser or rewrite limit -- a real bug worth investigating
before 10c lands.

If 10b shows `wolfram_sheffer_commutativity` returns PROVED
under our 32-step budget (unlikely but possible if the
unfailing fallback discovers the right lemma early), document
the surprise and revisit whether a harder conjecture is
needed (e.g., associativity-of-OR, derivable from NAND).

If the Wolfram axiom's nested-nand structure overflows the
parser's term-depth limits (`WALD_MAX_ARITY` is 8 but term
depth is unbounded), document the bound and revisit.

## Out of scope

- Deriving full Sheffer's three axioms from the Wolfram axiom
  (that's McCune 2002's whole paper -- weeks of saturation
  work).
- AC-aware NAND joinability (would also dissolve
  `comm_monoid_swap`'s QUEUE_EMPTY).
- Comparison against EQP / Otter on the same conjecture
  (Twee is our reference prover).

## Verification

Documentation-only resolution.  No `make test` / `make wl-test`
impact; both stay green from the prior 9.4c landing
(166/166 C, 323 WL).  10b is the implementation step that
exercises the parser + bench harness.
