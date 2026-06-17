---
Template: Symbol
Name: TProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TProofObject
Keywords: [proof object, equational, ATP, ProofObject, proof dataset, native proof]
SeeAlso: [TToProofObject, TFindProof, TFindEquationalPath, ProofObject, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TProofObject]()[*data*]</code> is a native thvm proof object: *data* is an Association carrying the proof's `"ProofType"`, the held `"Theorem"` and `"Axioms"`, the `"Variables"` / `"Constants"` signature, the keyed proof `"Steps"` DAG, and a `"Status"`.

<code>[TProofObject]()[*data*][*prop*]</code> returns a property of the proof.

## Details & Options

- `TProofObject` is thvm's own proof object, built directly from the engine's trace without round-tripping symbols through a kernel reconstruction. The `"ProofType"` field is a discriminator (`"Equational"` today) so other proof kinds can share the head later.
- [TFindProof]() returns a `TProofObject` instead of a built-in `ProofObject` when called with the option `"ProofForm" -> "TProofObject"` (the default is `"ProofObject"`).
- It mirrors the `ProofObject` property interface, so *prop* may be `"ProofType"`, `"Theorem"` (or `"Theorems"`), `"Axioms"`, `"Variables"`, `"Constants"`, `"Status"`, `"Statistics"`, `"ProofAssociation"`, `"ProofDataset"`, `"ProofLength"`, `"ProofGraph"`, `"ProofFunction"`, `"ProofObject"`, or `"Properties"`.
- The proof `"Steps"` are the same per-step shape the built-in `ProofObject`'s `ProofDataset` uses: each `{Type, n}` key maps to `<|"Statement" -> ..., "Proof" -> <|"Input", "Construct", "Position", "Rule", "Orientation", "Side"|>|>` (`Axiom` / `Hypothesis` rows carry an empty `"Proof"`).
- [TToProofObject]() converts a `TProofObject` to the genuine built-in `ProofObject` it shadows; the `"ProofObject"` property does the same.

## Basic Examples

Ask [TFindProof]() for a native proof object:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20]
```
<!-- => TProofObject[<|"ProofType" -> "Equational", "Status" -> "Proved", ...|>] -->

## Scope

The property accessors mirror the built-in `ProofObject`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20];
{p["ProofType"], p["Status"], p["ProofLength"]}
```
<!-- => {"Equational", "Proved", 2} -->

---

The keyed proof DAG is available as the `"ProofAssociation"`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20];
Keys @ p["ProofAssociation"]
```
<!-- => {{Axiom, 1}, {Hypothesis, 1}, {SubstitutionLemma, 1}, {Conclusion, 1}} -->

## Properties and Relations

The proof verifies through the same `ProofFunction` interface a built-in `ProofObject` exposes:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20];
p["ProofFunction"][p["Theorems"]]
```
<!-- => Success["EquationalProof", <|...|>] -->

---

[TToProofObject]() converts back to the built-in `ProofObject`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "ProofForm" -> "TProofObject", TimeConstraint -> 20];
Head @ TToProofObject[p]
```
<!-- => ProofObject -->

## Possible Issues

`"ProofForm"` defaults to `"ProofObject"`, so existing code is unaffected; you opt in to the native object explicitly:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
Head @ TFindProof[
    ForAll[{a, b}, a \[CircleTimes] b == b \[CircleTimes] a],
    "AbelianGroupAxioms", TimeConstraint -> 10]
```
<!-- => ProofObject - the default form is unchanged -->
