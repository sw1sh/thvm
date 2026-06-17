---
Template: Symbol
Name: TAtpDescribeMethod
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpDescribeMethod
Keywords: [ATP, Method, preset, introspection]
SeeAlso: [TAtpSchedule, TFindProof, TRelevantAxioms]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpDescribeMethod]()[$method$]</code> returns an Association describing what a `Method` spec resolves to.  Useful for verifying that a list-form spec like `{"Twee", "Ordering" -> "LPO"}` lands the suboptions you expect on the C engine.

## Details & Options

- A named preset (`"Waldmeister"`, `"VampireUEQ"`, `"Twee"`, `"EProver"`, `"VampireRandom"`) returns the preset's full defaults Association: the suboptions the dispatcher merges with the user's overrides.
- A list spec like `{"Twee", "Ordering" -> "LPO"}` returns the preset's defaults merged with the user's overrides - the options that actually reach the C engine, so you see what wins on collision.
- A non-preset config like `{"Completion", "Ordering" -> "LPO"}` returns the bare Association of suboptions, with no preset merge.
- `Automatic` / `"Portfolio"` / `"VampirePortfolio"` / `"VampirePortfolioCompact"` return `<|"Schedule" -> {...}|>` describing the multi-entry rotation rather than a single config.

## Basic Examples

A named preset returns its defaults:

```wl
TAtpDescribeMethod["Waldmeister"]
```
<!-- => <|"CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True, "SelectionRatio" -> 51, ...|> -->

List form shows the merged result:

```wl
TAtpDescribeMethod[{"Twee", "Ordering" -> "LPO"}]
```
<!-- => <|"CriticalPairWeight" -> "Twee", "Ordering" -> "LPO", "GroundJoin" -> True, ...|> -->

A portfolio returns a Schedule key:

```wl
TAtpDescribeMethod["VampirePortfolio"]
```
<!-- => <|"Schedule" -> {{"VampireUEQ"}, ...}|> -->

## Properties & Relations

- [TAtpSchedule]() is the companion: given the same `Method` spec, it returns the list of entries the dispatcher would try.  Pair the two to inspect both shape (schedule) and content (per-entry options) of a portfolio.
- [TFindProof]() with `Method -> $method$` runs the configuration this Association describes.
