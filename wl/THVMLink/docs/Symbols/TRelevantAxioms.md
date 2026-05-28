---
Template: Symbol
Name: TRelevantAxioms
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TRelevantAxioms
Keywords: [ATP, axiom relevance, premise selection, SInE, filter]
SeeAlso: [TFindProof, TAtpSchedule, TAtpDescribeMethod]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TRelevantAxioms]()[$conjecture$, $axioms$]</code> reports which axioms the relevance filter keeps vs. drops for proving $conjecture$, without running a proof.  Makes the filter transparent so you can verify a setting before committing to a long portfolio run.

<code>[TRelevantAxioms]()["$Theorem$", "$Theory$"]</code> resolves names through [AxiomaticTheory]().

Returns <code><|"Mode" -> ..., "Kept" -> {axioms}, "Dropped" -> {<|"Axiom" -> _, "Symbols" -> _, "Reason" -> _|>, ...}|></code>.  The relevance mode is set by the `Method` `"AxiomRelevance"` suboption.

## Details & Options

Modes (passed via `Method -> {"AxiomRelevance" -> $mode$}`):

- `None` - keep all axioms.
- `"Safe"` (default) - drop only provably dead-weight axioms (a confined symbol occurring on both sides; sound and completeness-preserving).  The Y combinator dropped when the goal is Y-free is a typical case.
- `"Connected"` or `{"Connected", "FrequencyCutoff" -> $f$, "MaxGenerations" -> $n$}` - symbol-reachability pruning.  Coarse heuristic; can drop a needed axiom.
- `"SInE"` or `{"SInE", "SineTolerance" -> $st$, "SineDepth" -> $sd$, "SineGenerality" -> $sgt$}` - the Hoder-Voronkov SInE premise selection as it ships in Vampire (D-relation + bounded BFS from the conjecture's symbols).  Defaults `3 / 2 / 8` mirror Vampire's `--sine_tolerance / --sine_depth / --sine_generality_threshold`.

## Examples

### Basic examples

The default `"Safe"` mode on AbelianGroup commutativity:

```wl
TRelevantAxioms[
    Inactive[Equal][x*y, y*x],
    "AbelianGroupAxioms",
    Method -> {"AxiomRelevance" -> "Safe"}]
```
<!-- => <|"Mode" -> "Safe", "Kept" -> {...}, "Dropped" -> {<|"Axiom" -> _, "Symbols" -> _, "Reason" -> _|>, ...}|> -->

SInE pruning on a many-axiom theory:

```wl
TRelevantAxioms["ImpliesWolframAxioms", "MeredithAxioms",
    Method -> {"AxiomRelevance" -> "SInE"}]
```
<!-- => <|"Mode" -> "SInE", "Kept" -> {<reachable axioms>}, "Dropped" -> {...}|> -->

## Properties & Relations

- [TFindProof]() with `Method -> {..., "AxiomRelevance" -> $mode$}` runs the same filter before the saturator sees the axiom list.  This entry inspects the partition without running the proof.
- `Automatic` (the [TFindProof]() default) appends a SInE-pruned variant to the portfolio tail for axiom sets of 8+ entries.

## Possible Issues

- `"Connected"` is heuristic: it can drop an axiom needed to close the goal, in which case the proof attempt fails.  Use `"Safe"` (sound and completeness-preserving) when you do not know whether the heuristic will work.
- The `Dropped` partition entries carry a `"Reason"` string explaining why each axiom was filtered out.  Useful for diagnosing a heuristic miss.
