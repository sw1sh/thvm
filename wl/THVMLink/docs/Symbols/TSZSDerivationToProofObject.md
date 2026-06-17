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

Lift a parsed SZS derivation produced by a CLI wrapper:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
res = TVampireProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 10];
TSZSDerivationToProofObject[res["Inferences"]]
```
<!-- => <|"Backend" -> "SZS", "Steps" -> {...}, ...|> (the thvm-shaped proof dataset) -->

## Properties & Relations

- [TVampireProofObject](), [TEproverProofObject](), and [TWaldmeisterProofObject]() call this to convert a parsed derivation into the proof shape. [TTweeProofObject]() bypasses it because Twee emits no `fof` inference DAG.
- [TFindProof]() produces the same proof-dataset shape directly from the internal C engine; this builder makes the external-prover output structurally comparable to it.
