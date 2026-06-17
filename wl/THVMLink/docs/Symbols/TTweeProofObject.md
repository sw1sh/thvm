---
Template: Symbol
Name: TTweeProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTweeProofObject
Keywords: [Twee, ATP, ProofObject, SZS, TSTP, external prover, proof lift, theorem proving]
SeeAlso: [TTweeProof, TSZSDerivationToProofObject, TVampireProofObject, TEproverProofObject, TWaldmeisterProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTweeProofObject]()[*theory*, *thm*]</code> runs the Twee CLI via [TTweeProof]() and returns a `thvm`-shaped proof Association.

It is the `Method -> "TweeProcess"` dispatch target in [TFindProof]().

## Details & Options

- Twee's `--tstp` output is SZS-framed, but its proof body is a human-readable equation chain rather than TPTP `fof` inferences, so the lemma-list shape is built directly rather than via [TSZSDerivationToProofObject](). The dataset is keyed by `{"Axiom", n}` and `{"Lemma", n}` only, with no per-step construct-class metadata.
- Options:
  - `TimeConstraint`, `"Binary"` - forwarded to [TTweeProof]().
- When the Twee binary is not installed, the builder returns a `Failure["ExternalNoProof", ...]` rather than raising an error.

## Basic Examples

Lift a Twee proof and read its head:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
Head @ TTweeProofObject["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 10]
```
<!-- => Association (the thvm-shaped proof dataset) -->

## Properties & Relations

- [TTweeProof]() is the raw CLI wrapper this builds on.
- [TVampireProofObject](), [TEproverProofObject](), and [TWaldmeisterProofObject]() are the sibling external-prover lifts; unlike those three, [TTweeProofObject]() does not route through [TSZSDerivationToProofObject]() because Twee emits no `fof` inference DAG.
- [TFindProof]()`[..., Method -> "TweeProcess"]` is the in-portfolio way to invoke this.

## Possible Issues

- Because Twee carries no per-step inference DAG, the result lacks the `{"CriticalPairLemma", n}` / `{"SubstitutionLemma", n}` construct-class metadata the SZS-based lifts provide; for that, use [TVampireProofObject]() or [TEproverProofObject]().
