---
Template: Symbol
Name: TSZSDerivationToProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSZSDerivationToProofObject
Keywords: [SZS, TPTP, ATP, ProofObject, derivation, inference DAG, proof lift, external prover]
SeeAlso: [TVampireProofObject, TEproverProofObject, TWaldmeisterProofObject, TTweeProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSZSDerivationToProofObject]()[*derivation*]</code> builds a `thvm`-shaped proof Association from a parsed SZS *derivation* - the list returned by `Wolfram`Parser`TPTPImport[..., "SZS"]`.

## Details & Options

- Works for any ATP that emits SZS-framed `fof`-plus-`inference` output (Vampire, E, iProver, Twee `--tstp`, Otter, and so on). It is the shared lift the external-prover builders thread their parsed derivations through.
- The SZS-rule to `thvm`-construct mapping comes from `$SZSRuleToConstruct`, an Association from SZS inference-rule names (`superposition`, `forward_demodulation`, ...) to `thvm` `ProofObject` construct types (`CriticalPairLemma`, `SubstitutionLemma`, ...). Edit that Association to support a prover's idiosyncratic inference rule; unmapped rules fall through to `SubstitutionLemma`.
- Each inference step lands under one of the same `ProofDataset` construct keys the internal engine uses (`{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}`, `{"Conclusion", 1}`).
- Options:
  - `ParseFormulas` (default `False`) - when `True`, formula bodies are parsed into Wolfram expressions instead of kept as raw SZS strings.

## Basic Examples

Lift the parsed SZS derivation of a Vampire run on an inline TPTP problem, and read the shape of the result:

```wl
problem = Export[FileNameJoin[{$TemporaryDirectory, "group_inv_inv.p"}], "
cnf(associativity, axiom, mult(mult(X, Y), Z) = mult(X, mult(Y, Z))).
cnf(left_identity, axiom, mult(e, X) = X).
cnf(left_inverse, axiom, mult(i(X), X) = e).
cnf(goal, negated_conjecture, i(i(a)) != a).", "Text"];
res = TVampireProof[problem, TimeConstraint -> 10];
Keys @ TSZSDerivationToProofObject[res["Inferences"]]
```
<!-- => {Backend, Status, Goal, Axioms, ProofDataset, ProofLength, RuleHistogram} -->

## Properties & Relations

- [TVampireProofObject](), [TEproverProofObject](), and [TWaldmeisterProofObject]() call this to convert a parsed derivation into the proof shape. [TTweeProofObject]() bypasses it because Twee emits no `fof` inference DAG.
- [TFindProof]() produces the same proof-dataset shape directly from the internal C engine; this builder makes the external-prover output structurally comparable to it.
