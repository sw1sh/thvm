---
Template: Symbol
Name: TFindEquationalPath
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindEquationalPath
Keywords: [equational path, rewriting, directed rules, oriented rules, replacement path, witnessing path, FindEquationalProof]
SeeAlso: [TFindProof, TFindStringProof, TStringPath, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindEquationalPath]()[*thm*, *axioms*]</code> proves *thm* over *axioms* and returns the witnessing rewrite path: the list of terms from the theorem's left side to its right side, where consecutive entries differ by one rewrite.

<code>[TFindEquationalPath]()[*proof*]</code> re-walks an already-computed `ProofObject` into the same path.

<code>[TFindEquationalPath]()[*thm*, *axioms*, *prop*]</code> returns the property *prop* of the replacement path instead of the default `"Path"`.

## Details & Options

- The path is assembled left-chain forward, then right-chain reversed through the shared normal form - the join the equational proof witnesses. The path machinery is the Wolfram Function Repository <code>[ResourceFunction]()["FindEquationalPath"]</code>, re-homed into the paclet and run on the `ProofObject` [TFindProof]() emits.
- An axiom written with [Rule]() (`a -> b`) is installed *pre-oriented* - it fires left-to-right only. With one-sided axioms and the goal stated *source* `->` *target*, the path is a pure forward replacement chain, the behaviour of a hypothetical `FindReplacePath`. See the [Finding Replacement Paths](paclet:WolframInstitute/THVMLink/tutorial/FindReplacePath) tech note.
- An axiom written with [Equal]() (`a == b`), a two-element list, or [TwoWayRule]() (`a <-> b`) is a two-way equation; the engine orients it by the reduction ordering.
- *prop* is a single property name, `All` (every property, as an Association), or a list of names (the corresponding sub-Association). The properties are:
  - `"Path"` (default) - the list of terms from lhs to rhs.
  - `"Rules"` - the rule applied at each step.
  - `"Rewrites"` - the concrete <code>[ReplaceAt]()</code> / <code>[Replace]()</code> operation at each step (applying it to the step's term yields the next).
  - `"Substitutions"` / `"Bindings"` - the per-step variable bindings.
  - `"Justification"` - a `{lemma, orientation, position}` triple per step.
  - `"RewriteTest"` - a `Success` / `Failure` checking the rewrite sequence reproduces the path.
  - `"ProofObject"` - the underlying object.
- Options: the proof-building options of <code>[TFindProof]()</code> (`MaxSteps`, `TimeConstraint`, `Method`) for the *thm*, *axioms* form, plus the path options `"Reverse"`, `"Simplify"`, `"Canonicalize"`, `"TerminalLemmas"`, and `"Cache"`.
- Returns `$Failed` when the goal is not proved or no rewrite chain was recorded.

## Basic Examples

A one-sided associativity rule and the path that fully right-associates a product:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20]
```
<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

## Scope

The path can be recovered from a `ProofObject` you already hold; the single-argument form re-walks its proof dataset:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20];
TFindEquationalPath[p]
```
<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

## Applications

`All` returns every property of the path as one Association - the path plus the full provenance of each step:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
Keys @ TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    All, TimeConstraint -> 20]
```
<!-- => {ProofObject, RewriteTest, Justification, Rewrites, Rules, Substitutions, Bindings, Path} -->

---

`"Rewrites"` gives the concrete replacement applied at each step; applying them in sequence reconstructs the path:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "Rewrites", TimeConstraint -> 20]
```
<!-- => {Replace[((a ⊗ b) ⊗ c) ⊗ d :> (a ⊗ b) ⊗ (c ⊗ d)], Replace[(a ⊗ b) ⊗ (c ⊗ d) :> a ⊗ (b ⊗ (c ⊗ d))]} -->

---

`"Justification"` names the lemma, orientation, and position of each rewrite:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "Justification", TimeConstraint -> 20]
```
<!-- => {{{Axiom, 1}, Right, {}}, {{Axiom, 1}, Right, {}}} -->

## Properties and Relations

With one-sided axioms the path is a genuine forward replacement chain: every consecutive pair is a legal left-to-right application of the rule, which [ReplaceList]() confirms:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
path = TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20];
And @@ (MemberQ[ReplaceList[#[[1]], (x_ \[CircleTimes] y_) \[CircleTimes] z_ :> x \[CircleTimes] (y \[CircleTimes] z)], #[[2]]] & /@ Partition[path, 2, 1])
```
<!-- => True -->

## Possible Issues

Equational provability is symmetric, so a goal stated against the reduction *does* still prove (the sides are joinable) - the path simply traverses the chain in reverse. For a forward replacement chain, state the goal *source* `->` *target* with *source* the term you start from. An unproved goal returns `$Failed`:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindEquationalPath[
    ForAll[{a, b}, a \[CircleTimes] b == b \[CircleTimes] a],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 5]
```
<!-- => $Failed - commutativity is not derivable from associativity alone -->
