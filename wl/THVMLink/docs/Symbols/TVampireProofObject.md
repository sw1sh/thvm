---
Template: Symbol
Name: TVampireProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TVampireProofObject
Keywords: [Vampire, ATP, ProofObject, SZS, external prover, proof lift, theorem proving]
SeeAlso: [TVampireProof, TSZSDerivationToProofObject, TTweeProofObject, TEproverProofObject, TWaldmeisterProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TVampireProofObject]()[*theory*, *thm*]</code> runs the Vampire CLI via [TVampireProof]() and converts the result through [TSZSDerivationToProofObject]() into a `thvm`-shaped proof Association.

It is the `Method -> "VampireProcess"` dispatch target in [TFindProof](), so proofs from differing Methods (an internal preset versus the external Vampire CLI) can be compared structurally.

## Details & Options

- The returned Association lands each inference step under the same `ProofDataset` construct keys the internal engine uses (`{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}`, `{"Conclusion", 1}`), with `"Backend" -> "SZS"`.
- Options:
  - `TimeConstraint`, `"Mode"`, `"Binary"` - forwarded to [TVampireProof]().
  - `"ParseFormulas"` (default `False`) - when `True`, each step's `Statement` is `TPTPImport`-parsed into a Wolfram expression so structural comparison against the internal engine's output works (slow, roughly 5 seconds per formula).
  - `"LiftToProofObject"` (default `False`) - when `True`, wraps the Association into a literal `ProofObject` head (implying `"ParseFormulas" -> True`), so the standard property accessors `p["ProofFunction"]`, `p["ProofGraph"]`, `p["ProofLength"]`, and `p["Theorems"]` work.
- When the Vampire binary is not installed, the builder returns a `Failure["ExternalNoProof", <|"Tool" -> _, "Status" -> _, "Seconds" -> _|>]` rather than raising an error, so a fresh-checkout build does not break.

## Basic Examples

Lift a Vampire proof into a literal `ProofObject`:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TVampireProofObject["AbelianGroupAxioms", "InverseOfInverse",
    "LiftToProofObject" -> True, TimeConstraint -> 10]
```
<!-- => ProofObject[...] -->

## Properties & Relations

- [TVampireProof]() is the raw CLI wrapper this builds on; [TVampireProofObject]() is the proof-shaped lift.
- [TTweeProofObject](), [TEproverProofObject](), and [TWaldmeisterProofObject]() are the sibling external-prover lifts; all four route through [TSZSDerivationToProofObject]() except Twee.
- [TFindProof]()`[..., Method -> "VampireProcess"]` is the in-portfolio way to invoke this.

## Possible Issues

- Without `"LiftToProofObject" -> True` the result is a raw Association keyed by `"Status"`, `"Backend"`, and the per-step construct keys, not a `ProofObject` head; the property accessors only apply after the lift.
