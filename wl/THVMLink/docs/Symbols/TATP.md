---
Template: Symbol
Name: TATP
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TATP
Keywords: [ATP, raw saturator, Waldmeister, .pr file, statistics]
SeeAlso: [TFindProof, TRelevantAxioms, TAtpSchedule]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TATP]()[$axioms$, $conjecture$]</code> runs the IC-native ATP saturation on the given equational axioms and conjecture, returning an Association with `"Status"`, `"Steps"`, `"Rules"`, `"QueueSize"`.  Variables are written as patterns (`x_`); the engine treats them as universally-quantified meta-variables.

<code>[TATP]()[File["$path$.pr"]]</code> parses a Waldmeister `.pr` spec file via the C-side `wald_parse_file` and runs the saturator directly.

## Details & Options

- [TATP]() is the lower-level cousin of [TFindProof](): same C engine, simpler return shape.  Use it when you want the raw saturation statistics without the `ProofObject` reconstruction pass.
- Method options accepted by [TFindProof]() are accepted here too (the dispatch goes through the same WL surface).
- `Witness -> {$x_, ...$}` captures the existential bindings on a successful proof.  `AllWitnesses -> True` enumerates bounded existential proofs with `MaxDepth` / `MaxWitnesses` caps.

## Examples

### Basic examples

A small group-theory saturation, no goal (returns the derived rules):

```wl
TATP[
    {x_*1 == x_,
     x_*inv[x_] == 1,
     x_*(y_*z_) == (x_*y_)*z_},
    x_*y_ == y_*x_]
```
<!-- => <|"Status" -> "Proved", "Steps" -> _Integer, "Rules" -> _Integer, "QueueSize" -> _Integer|> -->

A Waldmeister `.pr` benchmark file:

```wl
TATP[File["wolfram.pr"]]
```
<!-- => <|"Status" -> "Proved", ...|> -->

## Properties & Relations

- [TFindProof]() wraps the same engine with a verifier-shaped `ProofObject` reconstruction pass.  Reach for [TFindProof]() when you want a real Wolfram `ProofObject` (the [FindEquationalProof]() interface).
- [TATP]() shares the [TRelevantAxioms]() filter and the `Method` / `TimeConstraint` / `MaxSteps` knobs with [TFindProof](); see [the AtpMethods tech note](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods) for the catalog.
- The `.pr` file driver is the same `wald_parse_file` entry the C-side bench harness uses, so a Waldmeister benchmark file runs identically here and at the C bench.
