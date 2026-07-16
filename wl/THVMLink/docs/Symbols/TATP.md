---
Template: Symbol
Name: TATP
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TATP
Keywords: [ATP, raw saturator, Waldmeister, .pr file, statistics]
SeeAlso: [TFindProof, TRelevantAxioms, TAtpSchedule]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TATP]()[$axioms$, $conjecture$]</code> runs the IC-native ATP saturation on the given equational axioms and conjecture, returning an Association with `"Status"`, `"Steps"`, `"Rules"`, `"QueueSize"`.  Variables are written as patterns (`x_`): universally quantified in an axiom, unification (witness) variables in the conjecture.

<code>[TATP]()[File["$path$.pr"]]</code> parses a Waldmeister `.pr` spec file via the C-side `wald_parse_file` and runs the saturator directly.

## Details & Options

- [TATP]() is the lower-level cousin of [TFindProof](): same C engine, simpler return shape.  Use it when you want the raw saturation statistics without the `ProofObject` reconstruction pass.
- `"Status"` is the engine's raw terminal tag: `"PROVED"`, `"REFUTED"`, `"TIMEOUT"` (the `MaxSteps` budget ran out), or `"QUEUE_EMPTY"` (saturated without reaching the goal).
- Options are `MaxSteps` (the selection budget, default 64), `Witness`, `AllWitnesses`, `MaxDepth`, and `MaxWitnesses`.  The [TFindProof]() options (`Method`, `TimeConstraint`, ...) are **not** accepted here - this is the raw fixed-strategy saturator.
- Unlike [TFindProof]() (which Skolemizes goal variables into constants for the universally-quantified reading), the raw saturator treats the conjecture's pattern variables as unification variables: `Witness -> {$x_, ...$}` captures their bindings on a successful proof, and `AllWitnesses -> True` enumerates bounded existential proofs with `MaxDepth` / `MaxWitnesses` caps.
- Write axioms over inert operators ([CircleTimes](), [CenterDot](), a plain head `f`).  An evaluating operator like [Times]() is `Flat` and `Orderless`, so the axioms mangle before the (held) encoding sees them.

## Basic Examples

Prove that the inverse is an involution, from the right-unit / right-inverse / associativity axioms:

```wl
TATP[
    {x_ \[CircleTimes] 1 == x_,
     x_ \[CircleTimes] inv[x_] == 1,
     x_ \[CircleTimes] (y_ \[CircleTimes] z_) == (x_ \[CircleTimes] y_) \[CircleTimes] z_},
    inv[inv[a]] == a]
```
<!-- => <|"Status" -> "PROVED", "Steps" -> 41, "Rules" -> 8, "QueueSize" -> 11|> -->

A Waldmeister `.pr` benchmark file:

```wl
TATP[File["tests/data/atp/wolfram_axiom_literal.pr"]]
```
<!-- => <|"Status" -> "PROVED", "Steps" -> 6, "Rules" -> 1, "QueueSize" -> 4|> -->

## Properties & Relations

- [TFindProof]() wraps the same engine with a verifier-shaped `ProofObject` reconstruction pass, the `Method` strategy surface, `TimeConstraint`, and the relevance filter.  Reach for [TFindProof]() when you want a real Wolfram `ProofObject` (the [FindEquationalProof]() interface); see [the AtpMethods tech note](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods) for the strategy catalog.
- The `.pr` file driver is the same `wald_parse_file` entry the C-side bench harness uses, so a Waldmeister benchmark file runs identically here and at the C bench.
