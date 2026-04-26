# ICC sort dispatch: resolution memo (stage 8.3d)

> Closes stage 8.3d.  Sibling of `ic_rule_dispatch.md` (8.3a)
> and `multi_sort.md` (8.4a).

## What 8.3d asked

Stage 8.3d's task description: *"If sort-checking proves useful
at dispatch time, wrap rules in BRI/ANN and verify the type-flow
rules let the wrong-sort branches collapse to ERA before APP-LAM
fires.  Scope TBD; may roll up under 8.4 (multi-sort) instead."*

The 8.3a design memo deferred 8.3d until 8.4 (multi-sort
signatures) landed: *"ICC TAG_BRI / TAG_ANN integration (8.3d)
deferred until 8.4 lands multi-sort signatures.  At that point,
an ANN-wrapped rule can short-circuit on sort mismatch, and the
integration becomes meaningful."*

## What 8.4 actually delivered

Stage 8.4 (closed in commit `e4cf0bf`) shipped:

1. **Sort metadata on `WaldSpec`** (8.4b): per-symbol arg/result
   sort indices; per-variable sort indices; sort name table.
2. **`wald_term_sort` / `wald_sort_check` helpers** (8.4c):
   top-down sort inference; rejects sort mismatches and unknown
   identifiers.
3. **Sort-check gating in saturation entry points** (8.4d):
   `thvm_atp_add_equation` and `thvm_atp_set_goal` reject ill-
   sorted inputs (mismatched lhs/rhs sorts or unknown
   identifiers).
4. **Multi-sort `.pr` fixture** (8.4e): `tests/data/atp/list_
   length.pr` exercises the full pipeline on a non-homogeneous
   signature.

The gating happens *at the saturation entry points*.  Once
inputs are well-sorted, all derived terms (CPs, normalized
forms, oriented rules) inherit well-sortedness through the
matching / unification / rewriting steps.

## Where ICC TAG_BRI / TAG_ANN would fit

Our IC has `TAG_BRI` (ICC bridge: θx.body) and `TAG_ANN` (ICC
annotation: {val : typ}).  Their reduction rules (per
`src/thvm.h:106-114`) implement ICC's type-flow:

- `APP (θx.body) arg = θx (APP body[x <- λ$k.x] ANN($k, arg))`
- `ANN val (λx.body) = λx ANN(APP val $k) body[x <- θ$k.x]`
- `ANN val (θx.body) = body[x <- val]   (type erasure)`

These are about **ICC types** (lambda-calculus dependent types)
-- not about first-order **sorts** in an equational signature.

To use ICC for FOL sort dispatch, we would need to:

1. Encode FOL sort ids as ICC type terms (possibly LAM
   constants).
2. Wrap each rule's RHS in an `ANN(rhs, sort_typ)`.
3. Wrap each rule's LHS in a `BRI(...)` so APP-BRI's
   type-backward flow could short-circuit on sort mismatch.

The encoding is a non-trivial research item.  And the practical
question: **what extra dispatch power would it buy us?**

## When does ICC sort dispatch buy something?

ICC type-flow short-circuits APP when the arg's annotated type
doesn't match the function's expected type.  In our setting,
this would matter only if **multiple rules share the same head
symbol** and need to be discriminated by argument sort
(operator overloading).

**Today our codebase doesn't have overloading.**  Each WaldSym
has one signature; rules are uniquely identified by their head
symbol.  Dispatch already discriminates correctly:

- `thvm_match(rule_lhs, target)` checks the head symbol; head
  symbol mismatch -> ERA via `prim_rewrite_step`.
- Argument-position sort mismatch is impossible *if the input
  was well-sorted to begin with*: well-sorted inputs +
  well-sorted rules -> well-sorted CPs (proved by induction on
  rewriting; matches what 8.4d's gate enforces).

The only scenarios where ICC sort dispatch would help:

- **Operator overloading**: same name `+` for both `nat -> nat
  -> nat` and `rat -> rat -> rat`.  Not in our `.pr` corpus.
- **Open-world dispatch**: rules added at runtime with no
  precheck.  Our `add_equation` checks at entry, so closed-world
  guarantees hold.
- **SupGen-style sort superposition** (stage 8.10): superpose
  alternative sort assignments over a single rule and let
  priority collapse pick.  This is research-grade and rolls
  under 8.10's domain.

## Decision

**8.3d closes with no IC code changes.**  The "wrong-sort
branches collapse" intent is satisfied by:

1. **Entry-point gating** (8.4d): ill-sorted equations / goals
   are rejected before they reach saturation.
2. **Closed-world inheritance**: well-sorted entry + well-typed
   rules -> well-sorted CPs.
3. **Head-symbol dispatch in `prim_rewrite_step`**: sort-
   discriminating signatures already get unique-by-head symbol
   dispatch.

ICC TAG_BRI / TAG_ANN integration becomes meaningful only with:
- Operator overloading (out of scope today; would need
  signature-level changes to allow multiple `WaldSym` entries
  with the same name and different sorts), or
- SupGen-style search (stage 8.10), where superposing sort
  assignments is the actual mechanism.

Both are documented as **follow-up research items**, not
prerequisites for the IC-native ATP arc.

## What this means in practice

If a future task needs ICC sort dispatch:

1. The infrastructure (`TAG_BRI`, `TAG_ANN`, their reduction
   rules) is already in place from earlier stages.
2. The encoding step (FOL sort -> ICC type) is the actual
   research work; not yet done.
3. The use case has to come from overloading or SupGen; without
   it, the wrapping is ceremony.

This memo records the analysis and unblocks future work; no
production code lands today.

## Verification

This is a documentation-only resolution.  No `make test` /
`make wl-test` impact; both remain green from the prior 8.4e
landing.
