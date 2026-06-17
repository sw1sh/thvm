---
Template: Symbol
Name: TToProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TToProofObject
Keywords: [proof object, conversion, ProofObject, equational, ATP]
SeeAlso: [TProofObject, TFindProof, ProofObject, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TToProofObject]()[*obj*]</code> converts a thvm [TProofObject]() *obj* to the built-in `ProofObject` it shadows (the logic tag, theorem, axioms, and the proof Association).

## Details & Options

- The conversion is structural: it re-wraps the same `"Theorem"`, `"Axioms"`, `"Variables"`, `"Constants"`, and proof `"Steps"` a [TProofObject]() holds into a genuine `ProofObject`, so the full built-in property API (`ProofGraph`, `ProofFunction`, `ProofLength`, `ProofNotebook`, ...) applies.
- The `"ProofObject"` property of a [TProofObject]() returns the same value: `obj["ProofObject"]` is `TToProofObject[obj]`.
- Use it to hand a thvm proof to code that expects a built-in `ProofObject`, or to fall back on the built-in verifier and notebook rendering.

## Basic Examples

Convert a native proof object to the built-in `ProofObject`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20];
Head @ TToProofObject[p]
```
<!-- => ProofObject -->

## Properties and Relations

The converted object verifies through the built-in `ProofFunction`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20];
po = TToProofObject[p];
po["ProofFunction"][po["Theorems"]]
```
<!-- => Success["EquationalProof", <|...|>] -->

---

It is the inverse of the lift [TFindProof]() applies internally, so converting and reading back the proof dataset round-trips:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20];
Keys @ Normal @ TToProofObject[p]["ProofDataset"] === Keys @ p["ProofAssociation"]
```
<!-- => True -->
