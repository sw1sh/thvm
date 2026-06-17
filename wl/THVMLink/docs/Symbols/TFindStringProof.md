---
Template: Symbol
Name: TFindStringProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindStringProof
Keywords: [string rewriting, semi-Thue, directed rules, oriented rules, bubble sort, FindStringProof, multiway]
SeeAlso: [TStringPath, TFindEquationalPath, TFindProof, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindStringProof]()[*thm*, *axioms*]</code> proves a string-rewriting theorem over the semi-Thue *axioms* and returns the `ProofObject`.

<code>[TFindStringProof]()[*thm*, *axioms*, *spec*]</code> returns the value selected by the [TFindProof]() return *spec* (`"Status"`, `"Path"`, `"Lemmas"`, ...).

## Details & Options

- Words are strings of letters; each character becomes one symbol and a word becomes a right-nested [CenterDot]() term (`"ABC"` encodes to `A·(B·C)`), with an associativity bridge axiom appended so a rule may fire at any position. The encoding follows the Wolfram Function Repository <code>[ResourceFunction]()["FindStringProof"]</code>.
- An axiom written `"BA" -> "AB"` is a [Rule]() and installs *pre-oriented*: one-directional string rewriting (a semi-Thue rule). An axiom written `{"BA", "AB"}`, `"BA" == "AB"`, or `"BA" <-> "AB"` is a two-way equation that the engine orients itself.
- A theorem is a word pair in any of those shapes; its direction is meaningless (the goal is an equation). A `List` of theorems is a multi-goal conjunction returning ONE `ProofObject` with a hypothesis/conclusion row pair per conjunct.
- Accepts the [TFindProof]() options `MaxSteps`, `TimeConstraint`, `Method`, `PortfolioFrontLoad`.
- The returned `ProofObject` tags its theory `StringLogic` (rather than `EquationalLogic`); it otherwise supports the full property API.

## Basic Examples

A single one-directional rule bubble-sorts a word; the proof is a real `ProofObject`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"}, TimeConstraint -> 30]
```
<!-- => ProofObject[<|"Theorem" -> ..., "Status" -> "Proved", ...|>] -->

The `"Status"` spec returns the bare decision:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindStringProof["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, "Status", TimeConstraint -> 30]
```
<!-- => Proved -->

## Scope

A list of theorems is one conjunction proved off a single saturation; the dataset carries a hypothesis per conjunct:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
po = TFindStringProof[{"BA" -> "AB", "BBA" -> "ABB"}, {"BA" -> "AB"}, TimeConstraint -> 30];
ContainsAll[Keys @ Normal @ po["ProofDataset"],
    {{"Hypothesis", 1}, {"Hypothesis", 2}, {"Conclusion", 1}, {"Conclusion", 2}}]
```
<!-- => True - one ProofObject, a hypothesis/conclusion row per conjunct -->

## Properties and Relations

The proof object verifies independently - pre-oriented rules carry a proper trace lineage through the engine, so the lift survives:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
po = TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"}, TimeConstraint -> 30];
po["ProofFunction"][]
```
<!-- => Success["EquationalProof", <|...|>] -->

---

A pair axiom is a two-way equation, the Wolfram Function Repository `FindStringProof` behaviour; a `Rule` axiom restricts rewriting to one direction. Both prove `"BBAA" == "AABB"`, but only the directed system keeps the word strictly sorting:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindStringProof[{"BBAA", "AABB"}, {{"BA", "AB"}}, "Status", TimeConstraint -> 30]
```
<!-- => Proved -->

## Possible Issues

Words must be nonempty strings of letters; anything else returns a `Failure` naming the offending word:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindStringProof["A1" -> "AB", {"A" -> "B"}, TimeConstraint -> 10]
```
<!-- => Failure["TATPParseError", <|"Word" -> "A1", "Reason" -> "expected a nonempty word of letters"|>] -->
