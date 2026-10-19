---
Template: Symbol
Name: TFindStringProof
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindStringProof
Keywords: [string rewriting, semi-Thue system, rewrite rules, word problem, bubble sort, FindStringProof, multiway system]
SeeAlso: [TStringPath, TFindEquationalPath, TFindProof, FindEquationalProof, ProofObject]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Finding Replacement Paths with Directed Rules](FindReplacePath)"]
---

## Usage

<code>[TFindStringProof]()[*thm*, *axioms*]</code> proves the string-rewriting theorem *thm* from the string *axioms*, and gives a [ProofObject]().

<code>[TFindStringProof]()[*thm*, *axioms*, "*spec*"]</code> gives the property *spec* of the proof search, as in [TFindProof]().

## Details & Options

- A word is a nonempty string of letters. Each letter becomes a symbol, and a word becomes a product of its letters under [CenterDot](), so `"ABC"` becomes `A·(B·C)`. An associativity axiom is added, so a rule can rewrite any part of a word.
- An axiom written as a rule `"BA" -> "AB"` rewrites from left to right only, as in a semi-Thue system. An axiom written as a pair `{"BA", "AB"}` or as `"BA" <-> "AB"` is an equation, oriented by the prover.
- A theorem is a pair of words in any of these forms. A list of theorems is proved as one conjunction, and gives one [ProofObject]().
- The proof is a [ProofObject]() over the words as products of letters, with all the properties of an equational proof. [TStringPath]() gives the rewrite path as a list of words.
- The values of *spec* are those of [TFindProof](), such as `"Status"`, `"Statistics"` and `"Path"`.
- [TFindStringProof]() gives `$Failed` when no proof is found, and a [Failure]() for input that is not a pair of words.
- [TFindStringProof]() takes the options of [TFindProof]():

| | | |
| --- | --- | --- |
| [MaxSteps]() | [Automatic]() | maximum number of critical pairs to process |
| [Method]() | [Automatic]() | proof search method |
| [TimeConstraint]() | [Infinity]() | time limit in seconds |

## Basic Examples

The rule `"BA" -> "AB"` sorts a word:

```wl
TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"}]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Ask for the status of the proof search:

```wl
TFindStringProof["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, "Status"]
```

<!-- => "Proved" -->

## Scope

An axiom given as a pair of words is an equation:

```wl
TFindStringProof["BBAA" -> "AABB", {{"BA", "AB"}}, "Status"]
```

<!-- => "Proved" -->

---

An axiom given with [TwoWayRule]() is an equation too:

```wl
TFindStringProof["BBAA" <-> "AABB", {"BA" <-> "AB"}, "Status"]
```

<!-- => "Proved" -->

---

A list of theorems is proved as one conjunction:

```wl
TFindStringProof[{"BA" -> "AB", "BBA" -> "ABB"}, {"BA" -> "AB"}]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

The `"Statistics"` spec gives the number of rewrite steps in the chain that closes the goal, here the eight steps of the path, and the numbers of rules, trace entries and queued critical pairs:

```wl
TFindStringProof["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, "Statistics"]
```

<!-- => <|"Status" -> "Proved", "Steps" -> 8, "Rules" -> 4, "Trace" -> 9, "QueueSize" -> 0|> -->

---

The `"Path"` spec gives the rewrite path over the products of letters:

```wl
TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"}, "Path"]
```

<!-- => {B · (B · (A · A)), B · (A · (A · B)), A · (B · (A · B)), A · (A · (B · B))} -->

---

The theorem of the proof is an equation between products of letters:

```wl
TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"}]["Theorems"]
```

<!-- => B · (B · (A · A)) == A · (A · (B · B)) -->

## Options

### MaxSteps

A small step budget stops the search before it finds the proof:

```wl
TFindStringProof["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, "Status", MaxSteps -> 1]
```

<!-- => "TimedOut" -->

### Method

Prove the theorem with the `"Waldmeister"` configuration:

```wl
TFindStringProof["ABB" -> "BBA", {"AB" -> "BA"}, "AppliedMethod", Method -> "Waldmeister"]
```

<!-- => "Waldmeister" -->

### TimeConstraint

A time limit bounds a search that does not end:

```wl
TFindStringProof["AB" -> "BA", {"ABA" -> "BAB"}, "Status", TimeConstraint -> 2]
```

<!-- => "TimedOut" -->

## Properties and Relations

Find a proof:

```wl
po = TFindStringProof["BBAA" -> "AABB", {"BA" -> "AB"}]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

The proof verifies through its own `"ProofFunction"`:

```wl
po["ProofFunction"][po["Theorems"]]
```

<!-- => Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>] -->

---

[TStringPath]() gives the rewrite path as a list of words:

```wl
TStringPath["BBAA" -> "AABB", {"BA" -> "AB"}]
```

<!-- => {"BBAA", "BAAB", "ABAB", "AABB"} -->

## Possible Issues

A theorem that does not follow from the axioms gives `$Failed`:

```wl
TFindStringProof["AB" -> "BA", {"AA" -> "A"}]
```

<!-- => $Failed -->

---

A word with a character that is not a letter gives a [Failure]():

```wl
TFindStringProof["A1" -> "AB", {"A" -> "B"}]
```

<!-- => Failure["TATPParseError", <|"Word" -> "A1", "Reason" -> "expected a nonempty word of letters"|>] -->

---

[Equal]() compares two different strings to [False](), so a pair of words written with `==` is not a theorem:

```wl
TFindStringProof["BBAA" == "AABB", {"BA" -> "AB"}]
```

<!-- => a Failure with "Item" -> False and "Reason" -> "expected a word pair: ..." -->
