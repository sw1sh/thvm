---
Template: Symbol
Name: TToProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TToProofObject
Keywords: [proof object, conversion, ProofObject, equational proof, ATP]
SeeAlso: [TProofObject, TFindProof, ProofObject, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TToProofObject]()[*proof*]</code> converts the [TProofObject]() *proof* to a [ProofObject]().

## Details & Options

- [TToProofObject]() puts the theorem, the axioms, the variables, the constants and the steps of *proof* into a [ProofObject](), so the properties of [ProofObject](), such as `"ProofGraph"`, `"ProofFunction"` and `"ProofNotebook"`, apply to it.
- The `"ProofObject"` property of a [TProofObject]() gives the same result.
- [TToProofObject]() returns unevaluated on anything that is not a [TProofObject]().

## Basic Examples

Find a proof as a [TProofObject]():

```wl
p = TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofForm" -> "TProofObject"]
```

<!-- => a TProofObject summary box: Type Equational, Status Proved, Steps 2 -->

Convert it to a [ProofObject]():

```wl
TToProofObject[p]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

## Scope

Convert a [TProofObject]() given by its data:

```wl
TToProofObject[TProofObject[<|"ProofType" -> "Equational", "Theorem" -> f[f[f[a]]] == f[a], "Axioms" -> {f[f[x_]] == x_}, "Variables" -> {x}, "Constants" -> {}, "Steps" -> <|{"Axiom", 1} -> <|"Statement" -> HoldForm[f[f[x]] == x], "Proof" -> <||>|>, {"Hypothesis", 1} -> <|"Statement" -> HoldForm[f[f[f[a]]] == f[a]], "Proof" -> <||>|>, {"Conclusion", 1} -> <|"Statement" -> HoldForm[f[a] == f[a]], "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, "Position" -> {}, "Rule" -> f[f[x_]] -> x, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> HoldForm[f[a] == f[a]]|>|>|>, "Status" -> "Proved"|>]]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

## Properties and Relations

Convert a proof found as a [TProofObject]():

```wl
po = TToProofObject[TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofForm" -> "TProofObject"]]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

The converted proof checks through the `"ProofFunction"` of [ProofObject]():

```wl
po["ProofFunction"][po["Theorems"]]
```

<!-- => Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>] -->

---

Find a proof:

```wl
p = TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofForm" -> "TProofObject"]
```

<!-- => a TProofObject summary box: Type Equational, Status Proved, Steps 2 -->

The conversion keeps the steps of the proof:

```wl
Keys[Normal[TToProofObject[p]["ProofDataset"]]] === Keys[p["ProofAssociation"]]
```

<!-- => True -->

The `"ProofObject"` property gives the same result:

```wl
TToProofObject[p] === p["ProofObject"]
```

<!-- => True -->

## Possible Issues

[TToProofObject]() returns unevaluated on a [ProofObject]():

```wl
TToProofObject[TFindProof[f[f[f[a]]] == f[a], {f[f[x_]] == x_}]]
```

<!-- => TToProofObject[ProofObject[...]] -->
