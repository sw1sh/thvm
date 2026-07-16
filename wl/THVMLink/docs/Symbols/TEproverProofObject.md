---
Template: Symbol
Name: TEproverProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEproverProofObject
Keywords: [E prover, eprover, ATP, ProofObject, SZS, external prover, proof lift, theorem proving]
SeeAlso: [TEproverProof, TSZSDerivationToProofObject, TVampireProofObject, TTweeProofObject, TWaldmeisterProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TEproverProofObject]()[*theory*, *thm*]</code> runs the E prover CLI on the canonical TPTP (Thousands of Problems for Theorem Provers) problem file and lifts the SZS proof into a `thvm`-shaped proof Association via [TSZSDerivationToProofObject]().

It is the `Method -> "EproverProcess"` dispatch target in [TFindProof]().

## Details & Options

- E's `--proof-object --tstp-format` emits the same SZS-framed `fof`-plus-`inference` DAG as Vampire's `--proof tptp`, so the lift path is shared with [TVampireProofObject](). Each step lands under the internal engine's `ProofDataset` construct keys (`{"Axiom", n}`, `{"CriticalPairLemma", n}`, ...), with `"Backend" -> "SZS"`.
- Options:
  - `TimeConstraint`, `"Binary"` - forwarded to [TEproverProof]().
  - `"ParseFormulas"` (default `False`) - when `True`, each step's `Statement` is `TPTPImport`-parsed into a Wolfram expression (slow).
  - `"LiftToProofObject"` (default `True`) - wraps the Association into a literal `ProofObject` head (implying `"ParseFormulas" -> True`), so `p["ProofFunction"]`, `p["ProofGraph"]`, `p["ProofLength"]`, and `p["Theorems"]` work.  Set it `False` for the raw construct-keyed Association.
- When the E binary is not installed, the builder returns a `Failure["ExternalNoProof", ...]` rather than raising an error.

## Basic Examples

On a machine without the E binary the builder degrades to a typed `Failure` (the `TEproverProof::noeprover` message fires along the way):

```wl
Head @ TEproverProofObject["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 10]
```
<!-- => Failure - Failure["ExternalNoProof", ...] when eprover is absent; ProofObject where it is installed -->

With E installed (`brew install eprover`), the same call returns the lifted proof: <code>[TEproverProofObject]()["AbelianGroupAxioms", "InverseOfInverse"]</code> is a literal `ProofObject` supporting the standard property accessors, exactly like [TVampireProofObject]().

## Properties & Relations

- [TEproverProof]() is the raw CLI wrapper this builds on.
- [TVampireProofObject](), [TTweeProofObject](), and [TWaldmeisterProofObject]() are the sibling external-prover lifts; E and Vampire share the SZS lift path.
- [TFindProof]()`[..., Method -> "EproverProcess"]` is the in-portfolio way to invoke this.

## Possible Issues

- With `"LiftToProofObject" -> False` the result is a raw Association, not a `ProofObject` head; the property accessors only apply to the lifted form.
