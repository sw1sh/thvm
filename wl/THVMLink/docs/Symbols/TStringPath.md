---
Template: Symbol
Name: TStringPath
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TStringPath
Keywords: [string rewriting, semi-Thue, directed rules, bubble sort, replacement path, rewrite path, FindStringProof]
SeeAlso: [TFindStringProof, TFindEquationalPath, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TStringPath]()[*thm*, *axioms*]</code> proves a string-rewriting theorem and returns the rewrite path decoded back to plain words: the list of words from the theorem's source to its target, each consecutive pair differing by one rewrite.

## Details & Options

- *thm* and *axioms* take the [TFindStringProof]() shapes: a word pair `"l" -> "r"` (a [Rule]() axiom installs pre-oriented, one-directional), `{"l", "r"}`, `"l" == "r"`, or `"l" <-> "r"` (a two-way equation).
- The path's *endpoints* are the theorem's two words; the *intermediate* words are the route the completion chose (one of several valid rewrite sequences, fixed by the engine's symbol precedence), with adjacent re-bracketing duplicates removed.
- The path is the `"Path"` return spec of [TFindStringProof]() with the [CenterDot]() words decoded to strings. Intermediate re-bracketings introduced by the position bridge decode to the same word, and adjacent duplicates are dropped.
- A `List` of theorems returns one path per conjunct.
- With one-sided axioms, the path stated *source* `->` *target* is a pure forward replacement chain - the [Replace]()-path semantics of a hypothetical `FindReplacePath`. See the [Finding Replacement Paths](paclet:WolframInstitute/THVMLink/tutorial/FindReplacePath) tech note.
- Accepts the [TFindProof]() options `MaxSteps`, `TimeConstraint`, `Method`. Returns `$Failed` when the goal is not proved.

## Basic Examples

The bubble-sort path under the single one-directional rule `"BA" -> "AB"`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TStringPath["BBAA" -> "AABB", {"BA" -> "AB"}, TimeConstraint -> 30]
```
<!-- => {BBAA, BAAB, ABAB, AABB} -->

## Scope

A longer scramble traces the whole sorting trajectory; every step swaps one `"BA"` to `"AB"`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, TimeConstraint -> 60]
```
<!-- => {BBBAAA, BBABAA, BBAAAB, BABAAB, BAABAB, BAAABB, ABAABB, AABABB, AAABBB} -->

A `List` of theorems returns one decoded path per conjunct, `{path$1, path$2, ...}`.

## Properties and Relations

The path only ever sorts: every step moves one or more `A`s left past a `B` (some steps fire a *derived* completion rule, a composition of the axiom), so the inversion count strictly decreases to zero. A jump of more than one - here `8` to `6` - is a composed step:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
path = TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, TimeConstraint -> 60];
Count[Subsets[Characters[#], {2}], {"B", "A"}] & /@ path
```
<!-- => {9, 8, 6, 5, 4, 3, 2, 1, 0} - strictly decreasing; the 8->6 jump is a composed step -->

## Possible Issues

The axiom's orientation constrains the rewrite relation, not joinability: equational provability is symmetric, so the *reversed* goal still proves, and its path traverses the same chain backwards (un-sorting the word). State the goal *source* `->` *target* with *source* the word you start from to get a forward chain:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TStringPath["AAABBB" -> "BBBAAA", {"BA" -> "AB"}, TimeConstraint -> 20]
```
<!-- => {AAABBB, AABABB, ABAABB, BAAABB, BAABAB, BABAAB, BBAAAB, BBABAA, BBBAAA} - the route un-sorts -->
