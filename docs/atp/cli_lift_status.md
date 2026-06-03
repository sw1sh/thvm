# CLI ProofObject Lift Status

The `LiftToProofObject -> True` option on `TWaldmeisterProofObject`
and `TVampireProofObject` returns a literal
`ProofObject["EquationalLogic", goal, axioms, data]` head that
WL's property machinery accepts. Per `tools/baselines/lift_telemetry.tsv`
(commit 3c27087d), all 8 easy AbelianGroup/Group/Boolean/Hillman/
Meredith cases produce a working ProofObject with `ProofFunction`
(head: Function), `ProofGraph` (head: Graph), and correct
`ProofLength`. None pass full `pf[Theorems]` verification.

## What works (commits ae041d90 ~ 73b18053)

### Structural wrap

`ProcessProofObject.wl::liftToProofObject` takes the Association
returned by `TSZSDerivationToProofObject` and produces a literal
4-arg ProofObject head via:

* `liftStringLeaves`: TPTPImport's `"x1"[]` / `"k1"[]` String-token
  leaves promoted to `Global` symbols
* `collectVarSymbols`: variable symbols detected by name-match
  `"x" ~~ DigitCharacter ..`
* `withVariablePatterns`: `Hold[Pattern][v, Blank[]]` wrapping
  (via `Hold` to avoid the `v -> Pattern[v, _]` self-cycle)
* `inactivateEqual`: arg3 axioms wrapped in `Inactive[Equal][...]`
* `holdEqual`: arg4 Proof Statements wrapped in `HoldForm[lhs == rhs]`

The pipeline-test diagnostic (`tools/baselines/lift_pipeline_test.wls`,
commit 5a14987a) confirms: feeding preset's data through this wrap
preserves verification (`pf[Theorems]` returns Success). So the
structural wrap is correct; verification failures come from
specific reconstructed field VALUES.

### SubstitutionLemma + Conclusion reconstruction (commits 57f3d05a + 05b29972)

`reconstructSingleRewrite` enumerates `(Side, ConstructSide,
Position)` triples. For each, takes Construct's chosen side as a
rewrite rule, applies it at Position in Input's chosen side, and
checks if the result matches Step.Statement (sorted). On success
emits Side / ConstructSide / Position / Orientation / Rule fields.

A `closureMode` branch handles SL/Conclusion entries whose Statement
collapsed to `True` (the final reduction step that makes both sides
syntactically equal): accept candidates whose rewritten equation
has `lhs === rhs`.

For Conclusion entries that omit `Input`, the implicit Input is
the Hypothesis. When Conclusion's Construct points at a True-step
SL, walk one level back to find the actual rewrite rule.

### CriticalPairLemma reconstruction (commit 75990471 + c2421267)

`reconstructSuperposition` enumerates `(swap, Side1, Orientation1,
Side2, Orientation2, Position)` cross-products. For each:

* Take a fresh copy of MatchingConstruct (`cplR<n>` Unique vars) so
  its names don't collide with Construct's identical-name vars
* Unify Construct's chosen side at Position with MatchingConstruct's
  rule LHS via `THVMLink`ATP`Private`cplUnify` (thvm's Robinson
  unifier)
* On match, compute the candidate critical pair and check via
  **conjoint** `cplUnify[List[a,b], List[c,d], allVars]` (single
  shared sigma) against Step.Statement up to alpha-equivalence

Coverage on AbelianGroup/InverseOfInverse: CPL 1 reconstructs;
CPL 2, CPL 3 don't.

On Group/InverseOfInverse: CPL 1 and CPL 5 reconstruct; CPL 2-4
don't (`tools/baselines/lift_telemetry.tsv`).

## What stays open: CPL 2/3 enumeration coverage

For the failing CPLs the (Side1, Orientation1, Side2, Orientation2,
Position) cross-product apparently doesn't find a candidate whose
rewritten equation matches Step.Statement (modulo alpha-equivalence).
WM's SZS `cp(I, side_i, J, side_j)` labels don't directly map to
`(host, applied)` the way the reconstruction assumes. Hypotheses:

1. WM may apply rewrite rules differently than the standard
   syntactic superposition (e.g. associativity-aware matching for
   nested CircleTimes terms)
2. The alpha-equivalence check may be too strict in one direction
   and miss valid pairings
3. A non-variable position interior to the host's LHS needs to be
   tried; current `nonVarPositions` may skip a valid Position

Bisect probe (commit 5a14987a) confirmed: a placeholder
`Pattern[x, Blank[]] -> x` (identity rule) for un-reconstructed
CPLs makes verification WORSE: the verifier attempts unification
with the real axiom and reports `Can't unify <axiom_lhs> with
<placeholder>`. So `Position -> {}` + identity-Rule is strictly
worse than leaving the entry skeleton-only. Placeholder fallback
reverted in commit 73b18053.

## Open puzzles for the next iter

1. **CPL 2/3 enumeration**. Find what specific superposition wmcli
   performs for these entries on AbelianGroup/InverseOfInverse and
   match the enumeration to it. Likely requires reading WM source
   code or tracing wmcli's debug output with an inference-level
   verbosity flag.

2. **Verifier introspection**. WL's `pf[Theorems]` is internal.
   When all entries DO reconstruct (which the lift already does on
   the simpler CPL 1 case), test whether verification passes. If
   it still fails, the field shape is still wrong in some way the
   bisect couldn't isolate without verifier source access.

3. **Statement closure mode**. SL 3 / Conclusion 1 on InverseOfInverse
   have `Statement -> True` (closure). The Conclusion correctly walks
   back through a True-step SL. SL 3's own reconstruction works.
   No known issue here but verify in the next iter when CPL 2/3 are
   fixed.

## Diagnostics

* `tools/baselines/lift_telemetry.tsv`: per-case `ProofObject` /
  `ProofFunction` / `ProofGraph` / `ProofLength` / Verify status
* `tools/baselines/metadata_diff.wls`: side-by-side preset vs CLI
  Proof field diff per construct key
* `tools/baselines/lift_pipeline_test.wls`: confirms structural
  wrap is correct (preset data through lift wrap → Success)
