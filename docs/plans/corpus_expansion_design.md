# Corpus expansion: design memo (stage 9.4a)

> Sibling of `multi_witness_design.md` (9.1a).  The 9.4 task picks
> 3-5 textbook UEQ candidates from GRP / RNG / LCL / LAT
> divisions, hand-encoded as `.pr` fixtures (we can't pull TPTP
> files inside a cron firing).  This memo picks the candidates,
> predicts status, and notes KBO-vs-LPO orientation.

## Goal

Stretch the bench corpus past the small group / monoid /
multi-sort fixtures we have today, so 9.4c's IC-vs-Twee
comparison has more interesting deltas.  Today's corpus:

- `monoid_right_id.pr`     -- PROVED, 2 steps
- `idempotent_nested.pr`   -- PROVED, 2 steps
- `list_length.pr`         -- PROVED, 2 steps (multi-sort)
- `group_right_inverse_to_e.pr` -- PROVED, 5 steps
- `group_commutative_inverse.pr` -- TIMEOUT @ 256
- `exists_inverse.pr`      -- PROVED via narrow, 1 step
- `exists_multi.pr`        -- PROVED via narrow, 0 steps
- `*_<id>.expect`          -- one per fixture

Mix today: 5 small PROVED, 1 large TIMEOUT, 2 existential.  Need
more variety in axiom shape (lattices, rings, combinators) and
at least one MEDIUM proof that exercises real saturation under
the 32-step bench budget.

## Selected candidates

Four picks: three tractable, one hard, spanning four divisions.

### 1. `comm_monoid_swap.pr` (GRP)

**Source.** Standard commutative-monoid axiomatization; a CASC-J
warm-up problem.

**Signature.** `f: ANY ANY -> ANY`, `e: -> ANY`, `a, b: -> ANY`.

**Axioms.**

- `f(x, e) = x`           (right identity)
- `f(x, y) = f(y, x)`     (commutativity)

**Conjecture.** `f(a, b) = f(b, a)`.

**Predicted status.** PROVED in 0-1 steps.  The commutativity
axiom is structurally symmetric under both KBO and LPO -- neither
side strictly dominates -- so it goes in via the unfailing two-
direction fallback.  Once both directions are in `R`, the goal-
check's rewrite-and-compare path normalizes both sides to a
canonical form and PROVED fires.

**KBO vs LPO.** Both orderings agree the right-identity is
`lhs > rhs` (structurally bigger).  Commutativity is unorientable
in both -- the unfailing 2-way path is the path that closes this.
Whether KBO and LPO behave identically here is part of what 9.4c
will check.

### 2. `lattice_absorb_simple.pr` (LAT)

**Source.** First absorption law of lattice theory (Burris &
Sankappanavar, *A Course in Universal Algebra*, ch. 1).

**Signature.** `meet: ANY ANY -> ANY`, `join: ANY ANY -> ANY`,
`a, b: -> ANY`.

**Axioms.**

- `meet(x, join(x, y)) = x`   (A1 absorption)
- `join(x, meet(x, y)) = x`   (A2 absorption)

**Conjecture.** `meet(a, join(a, b)) = a`.

**Predicted status.** PROVED in 0 steps.  The conjecture is a
literal instance of A1 with `x = a, y = b`; the goal-rewrite path
normalizes `meet(a, join(a, b)) -> a` directly.  The interesting
part is whether saturation reorders the axioms into a redundant
form before goal-check fires.

**KBO vs LPO.** `meet` and `join` need a precedence; we pick
`meet > join > a > b` so the absorption rules orient lhs > rhs
consistently.  Both orderings should give the same result.

### 3. `ring_distrib_zero.pr` (RNG)

**Source.** Distributivity of multiplication over addition with
right-identity zero, paraphrased from any introductory ring
chapter.

**Signature.** `mul: ANY ANY -> ANY`, `add: ANY ANY -> ANY`,
`zero: -> ANY`, `a, b: -> ANY`.

**Axioms.**

- `add(x, zero) = x`                                 (right id)
- `mul(add(x, y), z) = add(mul(x, z), mul(y, z))`    (right dist)

**Conjecture.** `mul(add(a, zero), b) = mul(a, b)`.

**Predicted status.** PROVED in 1-2 steps.  Right-identity
rewrites `add(a, zero) -> a` inside the lhs; both sides become
`mul(a, b)`.  The distributive axiom doesn't need to fire on this
particular goal but is included so the shape is recognisable as a
ring fragment (and so future bench expansions have a place to
land harder ring conjectures).

**KBO vs LPO.** Precedence `mul > add > zero > a > b`; both
axioms orient lhs > rhs under both.

### 4. `comb_K.pr` (LCL)

**Source.** Combinatory logic K rule (Schoenfinkel-Curry).

**Signature.** `app: ANY ANY -> ANY`, `K: -> ANY`, `S: -> ANY`,
`a, b, c: -> ANY`.

**Axioms.**

- `app(app(K, x), y) = x`                     (K)
- `app(app(app(S, x), y), z) =
   app(app(x, z), app(y, z))`                 (S)

**Conjecture.** `app(app(K, a), b) = a`.

**Predicted status.** PROVED in 1 step (literal K-rule
application).  Useful as a sanity check that the LCL division's
applicative encoding survives our parser and KBO config.

**KBO vs LPO.** Precedence `S > K > app > a > b > c`.  K's lhs
(`app(app(K, x), y)`, depth 2) clearly dominates rhs (`x`, var)
under both.

### 5. (HARD) `group_left_id_from_assoc.pr` (GRP)

**Source.** Classical exercise: derive left-identity from right-
identity + associativity + right-inverse.

**Signature.** Same as `group_commutative_inverse.pr` -- `e`,
`i: ANY -> ANY`, `f: ANY ANY -> ANY`, `a: -> ANY`.

**Axioms.**

- `f(x, e) = x`                       (right id)
- `f(x, i(x)) = e`                    (right inverse)
- `f(f(x, y), z) = f(x, f(y, z))`     (associativity)

**Conjecture.** `f(e, a) = a`.

**Predicted status.** TIMEOUT @ 32-step bench budget.
Theoretically derivable in ~6-12 steps via:
`f(e, a) = f(f(a, i(a)), a) = f(a, f(i(a), a)) = ... = a`,
but the prover needs to discover the lemma `f(i(x), x) = e`
(left-inverse) along the way.  Under unfailing KB without AC
hints this exceeds the 32-step bound; with the 256-step bound
group_commutative_inverse already TIMEOUTs.

This is the "stress test" entry -- reproduces the
`group_commutative_inverse` TIMEOUT pattern at half the conjecture
size, giving 9.4c data on whether the lemma-discovery cost is
constant or scales with conjecture depth.

## Aggregate prediction

| Fixture | Division | Predicted | Step bound | Observed (9.4b) |
|---|---|---|---|---|
| `comm_monoid_swap`        | GRP | PROVED  | 0-1    | QUEUE_EMPTY @ 2 |
| `lattice_absorb_simple`   | LAT | PROVED  | 0      | PROVED @ 0      |
| `ring_distrib_zero`       | RNG | PROVED  | 1-2    | PROVED @ 0      |
| `comb_K`                  | LCL | PROVED  | 1      | PROVED @ 0      |
| `group_left_id_from_assoc`| GRP | TIMEOUT | 32 cap | TIMEOUT @ 32    |

Four PROVED + one TIMEOUT.  Hits the >=1 PROVED + >=1 TIMEOUT
target.  Total fixture count after 9.4 = 12 (existing 7 + these 5).

**Surprise** (9.4b update): `comm_monoid_swap` returns
QUEUE_EMPTY, not PROVED.  The commutativity axiom is unorientable
and the unfailing 2-way fallback installs both `f(x, y) ->
f(y, x)` and `f(y, x) -> f(x, y)` -- those rewrite cycle
indefinitely under the goal-rewrite path, so neither side
canonicalises.  Saturation completes (queue empties at step 2)
without proving the goal.  This is the textbook AC-redundancy
gap; cleanly motivates an AC-aware joinability criterion as
future work.

## KBO-vs-LPO note

8.5d landed the empirical claim that "KBO and LPO agree on our
corpus".  These five candidates are picked specifically to keep
that property intact for 9.4b -- every axiom has a
straightforward `lhs > rhs` orientation under either ordering
(modulo the commutativity axiom in #1, which is unorientable
under BOTH).  9.4c will re-verify by running the bench under
both orderings; if a disagreement appears it's noteworthy.

## Stop conditions

If 9.4b finds that one of the four "PROVED" picks actually
TIMEOUTs under the 32-step bench budget, document the actual
result in the `.expect` file and update this memo's prediction
table.  This is the natural outcome of a stress-test corpus.

If 9.4b finds that the parser rejects one of the signatures
(e.g. multi-letter constructor names like `meet`, `join`, `add`,
`mul` aren't supported), shorten to single-letter aliases.

If `group_left_id_from_assoc` proves quickly (against
prediction), we either over-budgeted on `group_commutative_inverse`
or there's a hidden joinability shortcut -- worth investigating
in 9.4c.

## Out of scope

- Generic `.tptp` parser for direct TPTP file ingestion (9.2's
  WL surface handles `.pr`; full TPTP support is a separate
  arc).
- Twee-class redundancy criteria (ground joinability,
  connectedness): the corpus expansion stresses the existing
  pruning, not new pruning.
- AC-aware unification: the picks above are deliberately AC-free
  so the comparison stays clean.

## Verification

This is a documentation-only resolution.  No `make test` /
`make wl-test` impact; both remain green from the prior 9.3
landing (166 / 323).
