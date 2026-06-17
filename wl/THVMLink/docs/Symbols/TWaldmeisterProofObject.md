---
Template: Symbol
Name: TWaldmeisterProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWaldmeisterProofObject
Keywords: [Waldmeister, wmcli, ATP, ProofObject, proof protocol, external prover, proof lift, theorem proving]
SeeAlso: [TWaldmeisterProof, TSZSDerivationToProofObject, TVampireProofObject, TTweeProofObject, TEproverProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TWaldmeisterProofObject]()[*file*]</code> runs the local Waldmeister binary via [TWaldmeisterProof]() on a `.pr` problem *file* and converts the proof protocol through [TSZSDerivationToProofObject]() into a `thvm`-shaped proof Association.

<code>[TWaldmeisterProofObject]()[*theory*, *thm*]</code> resolves a pre-generated `.pr` file under `tools/baselines/wm_pr/` from the (*theory*, *thm*) pair.

It is the `Method -> "WaldmeisterProcess"` dispatch target in [TFindProof]().

## Details & Options

- Each protocol inference lands under the internal engine's `ProofDataset` construct keys (`{"Axiom", n}`, `{"CriticalPairLemma", n}`, ...), with `"Backend" -> "SZS"`.
- The `(theory, thm)` form returns a `NoCachedPr` `Failure` (carrying the converter command line) when the pre-generated `.pr` file is missing, because there is no in-process TPTP-to-`.pr` converter.
- Options:
  - `TimeConstraint`, `"Binary"`, `"MathlinkPath"` - forwarded to [TWaldmeisterProof]().
  - `"ParseFormulas"` (default `False`) - when `True`, each step's `Statement` is parsed into a Wolfram expression.
  - `"LiftToProofObject"` (default `False`) - when `True`, wraps the Association into a literal `ProofObject` head, so `p["ProofFunction"]`, `p["ProofGraph"]`, `p["ProofLength"]`, and `p["Theorems"]` work.
- When the `wmcli` binary is not installed, the builder returns a `Failure["ExternalNoProof", ...]` rather than raising an error.

## Basic Examples

Lift a Waldmeister proof into a literal `ProofObject`:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse",
    "LiftToProofObject" -> True, TimeConstraint -> 10]
```
<!-- => ProofObject[...] (or Failure["NoCachedPr", ...] when the .pr file is absent) -->

## Properties & Relations

- [TWaldmeisterProof]() is the raw CLI wrapper this builds on.
- [TVampireProofObject](), [TTweeProofObject](), and [TEproverProofObject]() are the sibling external-prover lifts.
- [TFindProof]()`[..., Method -> "WaldmeisterProcess"]` is the in-portfolio way to invoke this.

## Possible Issues

- The `(theory, thm)` form depends on a pre-generated `.pr` file (Waldmeister consumes its own `.pr` format, not TPTP); a missing file returns a `NoCachedPr` `Failure` with the generator command line rather than a proof.
