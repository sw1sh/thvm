---
Template: Symbol
Name: TFindEquationalProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindEquationalProof
Keywords: [theorem proving, ATP, equational logic, deprecated, alias]
SeeAlso: [TFindProof, FindEquationalProof, TFindEquationalPath]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindEquationalProof]()[*conjecture*, *axioms*]</code> is a deprecated name for <code>[TFindProof]()[*conjecture*, *axioms*]</code>.

<code>[TFindEquationalProof]()["*theorem*", "*theory*"]</code> is a deprecated name for <code>[TFindProof]()["*theorem*", "*theory*"]</code>.

## Details & Options

- [TFindEquationalProof]() takes the same arguments and options as [TFindProof]() and gives the same results.
- New code uses [TFindProof]().

## Basic Examples

Prove the double-inverse theorem of abelian groups:

```wl
TFindEquationalProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Prove a named theorem of a theory:

```wl
TFindEquationalProof["InverseOfComposite", "AbelianGroupAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

## Properties and Relations

The result is the same as that of [TFindProof]():

```wl
TFindEquationalProof["InverseOfComposite", "AbelianGroupAxioms", "Lemmas"] === TFindProof["InverseOfComposite", "AbelianGroupAxioms", "Lemmas"]
```

<!-- => True -->
