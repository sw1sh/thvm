---
Template: Symbol
Name: TStringPath
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TStringPath
Keywords: [string rewriting, semi-Thue system, rewrite rules, bubble sort, replacement path, rewrite path, FindStringProof]
SeeAlso: [TFindStringProof, TFindEquationalPath, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Finding Replacement Paths with Directed Rules](FindReplacePath)"]
---

## Usage

<code>[TStringPath]()[*thm*, *axioms*]</code> proves the string-rewriting theorem *thm* from the string *axioms*, and gives the list of words from one side of *thm* to the other, each obtained from the one before by a rewrite.

## Details & Options

- *thm* and *axioms* take the forms of [TFindStringProof](): a rule `"BA" -> "AB"` rewrites from left to right only, and a pair `{"BA", "AB"}` or `"BA" <-> "AB"` is an equation.
- The first and last words of the path are the two sides of *thm*. The words between them are the route the proof takes, one of several possible routes.
- A step can apply a rule derived by the proof, a combination of the axioms, so consecutive words can differ by more than one application of an axiom.
- With rule axioms and a theorem stated from a start word to an end word, the path is a chain of forward replacements. See [Finding Replacement Paths](paclet:WolframInstitute/THVMLink/tutorial/FindReplacePath).
- A list of theorems gives one path for each theorem.
- [TStringPath]() gives `$Failed` when no proof is found.
- [TStringPath]() takes the options of [TFindProof]():

| | | |
| --- | --- | --- |
| [MaxSteps]() | [Automatic]() | maximum number of critical pairs to process |
| [Method]() | [Automatic]() | proof search method |
| [TimeConstraint]() | [Infinity]() | time limit in seconds |

## Basic Examples

The path that sorts a word with the rule `"BA" -> "AB"`:

```wl
TStringPath["BBAA" -> "AABB", {"BA" -> "AB"}]
```

<!-- => {"BBAA", "BAAB", "ABAB", "AABB"} -->

## Scope

A longer word is sorted the same way:

```wl
TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"}]
```

<!-- => {"BBBAAA", "BBABAA", "BBAAAB", "BABAAB", "BAABAB", "BAAABB", "ABAABB", "AABABB", "AAABBB"} -->

---

With the axiom as an equation, the path can take another route:

```wl
TStringPath["BBAA" -> "AABB", {{"BA", "AB"}}]
```

<!-- => {"BBAA", "BABA", "BAAB", "ABAB", "AABB"} -->

---

A list of theorems gives one path for each:

```wl
TStringPath[{"BA" -> "AB", "BBA" -> "ABB"}, {"BA" -> "AB"}]
```

<!-- => {{"BA", "AB"}, {"BBA", "BAB", "ABB"}} -->

## Options

### MaxSteps

A small step budget leaves the theorem unproved:

```wl
TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, MaxSteps -> 1]
```

<!-- => $Failed -->

### Method

Prove the theorem with plain completion:

```wl
TStringPath["BBAA" -> "AABB", {"BA" -> "AB"}, Method -> "Completion"]
```

<!-- => {"BBAA", "BAAB", "ABAB", "AABB"} -->

### TimeConstraint

A time limit bounds a search that does not end:

```wl
TStringPath["AB" -> "BA", {"ABA" -> "BAB"}, TimeConstraint -> 2]
```

<!-- => $Failed -->

## Properties and Relations

The rewrite path:

```wl
path = TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"}]
```

<!-- => {"BBBAAA", "BBABAA", "BBAAAB", "BABAAB", "BAABAB", "BAAABB", "ABAABB", "AABABB", "AAABBB"} -->

Each step of the path moves letters `A` to the left of letters `B`, so the number of pairs `"B"` before `"A"` decreases along the path:

```wl
Count[Subsets[Characters[#], {2}], {"B", "A"}] & /@ path
```

<!-- => {9, 8, 6, 5, 4, 3, 2, 1, 0} -->

---

[TFindStringProof]() gives the proof the path is read from:

```wl
TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"}, "Status"]
```

<!-- => "Proved" -->

## Possible Issues

A theorem stated against the direction of a rule still proves, since equality is symmetric, and its path runs the rule backward:

```wl
TStringPath["AAABBB" -> "BBBAAA", {"BA" -> "AB"}]
```

<!-- => {"AAABBB", "AABABB", "ABAABB", "BAAABB", "BAABAB", "BABAAB", "BBAAAB", "BBABAA", "BBBAAA"} -->

---

A theorem that does not follow from the axioms gives `$Failed`:

```wl
TStringPath["AB" -> "BA", {"AA" -> "A"}]
```

<!-- => $Failed -->
