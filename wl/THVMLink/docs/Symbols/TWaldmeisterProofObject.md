---
Template: Symbol
Name: TWaldmeisterProofObject
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWaldmeisterProofObject
Keywords: [Waldmeister, wmcli, ATP, ProofObject, proof protocol, external prover, proof lift, theorem proving]
SeeAlso: [TWaldmeisterProof, TSZSDerivationToProofObject, TVampireProofObject, TTweeProofObject, TEproverProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TWaldmeisterProofObject]()[*file*]</code> runs the local Waldmeister binary via [TWaldmeisterProof]() on a `.pr` problem *file* and converts the proof protocol through [TSZSDerivationToProofObject]() into a `thvm`-shaped proof Association.

<code>[TWaldmeisterProofObject]()[*theory*, *thm*]</code> resolves a banked `.pr` file under `tools/baselines/wm_pr/` from the (*theory*, *thm*) pair when one is present, and otherwise GENERATES the `.pr` at runtime from the theory's symbolic axioms and the named [NotableTheorem]() conjecture.

It is the `Method -> "WaldmeisterProcess"` dispatch target in [TFindProof]().

## Details & Options

- Each protocol inference lands under the internal engine's `ProofDataset` construct keys (`{"Axiom", n}`, `{"CriticalPairLemma", n}`, ...), with `"Backend" -> "SZS"`.
- The `(theory, thm)` form uses a banked `.pr` under `tools/baselines/wm_pr/` as a byte-stable fast path; on a cache-miss it generates the `.pr` in-process (symbolic axioms and conjecture to TPTP CNF strings, then to `.pr` -- byte-identical to the banked construction) rather than failing.
- Options:
  - `TimeConstraint`, `"Binary"`, `"MathlinkPath"` - forwarded to [TWaldmeisterProof]().
  - `"ParseFormulas"` (default `False`) - when `True`, each step's `Statement` is parsed into a Wolfram expression.
  - `"LiftToProofObject"` (default `True`) - wraps the Association into a literal `ProofObject` head (implying `"ParseFormulas" -> True`), so `p["ProofFunction"]`, `p["ProofGraph"]`, `p["ProofLength"]`, and `p["Theorems"]` work.  Set it `False` for the raw construct-keyed Association.  The lifted proof renders and walks, but `p["ProofFunction"][p["Theorems"]]` does not verify: the protocol steps lack the rewrite metadata (Orientation / Rule / Side / Position) the verifier replays.
- When the `wmcli` binary is not installed, the builder returns a `Failure["ExternalNoProof", ...]` rather than raising an error.

## Basic Examples

Lift a Waldmeister proof into a literal `ProofObject` (the default):

```wl
TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 10]
```
<!-- => ProofObject[...] (the .pr is generated at runtime on a cache-miss; Failure["ExternalNoProof", ...] when wmcli is unavailable) -->

## Properties & Relations

- [TWaldmeisterProof]() is the raw CLI wrapper this builds on.
- [TVampireProofObject](), [TTweeProofObject](), and [TEproverProofObject]() are the sibling external-prover lifts.
- [TFindProof]()`[..., Method -> "WaldmeisterProcess"]` is the in-portfolio way to invoke this.

## Possible Issues

- The `(theory, thm)` form prefers a banked `.pr` file (Waldmeister consumes its own `.pr` format, not TPTP) for byte-stability, but no longer depends on one: a cache-miss is handled by generating the `.pr` at runtime. A non-equational conjecture (one Waldmeister's unit-equational format cannot express) returns a `Failure["WmGenerate", ...]`.
