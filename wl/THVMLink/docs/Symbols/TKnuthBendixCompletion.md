---
Template: Symbol
Name: TKnuthBendixCompletion
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKnuthBendixCompletion
Keywords: [Knuth-Bendix completion, unfailing completion, rewrite system, normal form, simplification, term rewriting, reduction ordering, Waldmeister]
SeeAlso: [TFindProof, ProofObject, AxiomaticTheory, FindEquationalProof, ReplaceRepeated, TwoWayRule]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Theorem Proving with THVMLink](ATP)"]
---

## Usage

<code>[TKnuthBendixCompletion]()[*axioms*]</code> completes the equations *axioms* to a rewrite system and gives a [ProofObject]() whose `"Theorems"` are the rules of the system.

<code>[TKnuthBendixCompletion]()[*axioms*, *term*]</code> gives the normal form of *term* under the completed system.

<code>[TKnuthBendixCompletion]()[*axioms*, {*s*<sub>1</sub>, *s*<sub>2</sub>, ...}]</code> completes under the lexicographic path ordering with precedence *s*<sub>1</sub> > *s*<sub>2</sub> > ....

<code>[TKnuthBendixCompletion]()[*axioms*, {{*s*<sub>1</sub>, *w*<sub>1</sub>}, {*s*<sub>2</sub>, *w*<sub>2</sub>}, ...}]</code> completes under the Knuth-Bendix ordering with weight *w*<sub>i</sub> for the symbol *s*<sub>i</sub> and precedence *s*<sub>1</sub> > *s*<sub>2</sub> > ....

<code>[TKnuthBendixCompletion]()[*axioms*, *term*, *order*]</code> gives the normal form of *term* under the system completed with the ordering *order*.

<code>[TKnuthBendixCompletion]()["*theory*", ...]</code> completes the axioms of [AxiomaticTheory]()["*theory*"].

## Details & Options

- [TKnuthBendixCompletion]() runs unfailing Knuth-Bendix completion, the saturation of the Waldmeister prover that [TFindProof]() also runs: it orients each equation into a rule by a reduction ordering, adds the critical pairs between rules as new equations, and repeats until no new equation remains.
- The *axioms* are equations, possibly quantified with [ForAll](), as [TFindProof]() takes them. A symbol bound by [ForAll]() is a variable; any other symbol is a constant.
- The `"Theorems"` of the result are the completed system. A rule `l -> r` rewrites an instance of `l` to the instance of `r`. An equation `l <-> r` that the ordering cannot orient rewrites an instance in whichever direction makes the term smaller in the ordering.
- When completion ends, the system is ground convergent: two terms over the symbols of the axioms are equal in the theory exactly when they have the same normal form. When it stops at the [TimeConstraint]() or [MaxSteps](), the normal form of a term is still equal to the term, but equal terms can have different normal forms.
- <code>[TKnuthBendixCompletion]()[*axioms*, *term*]</code> completes with the goal *term* == *c* for a fresh constant *c*, as Waldmeister's simplification does, and gives the normal form of *term* when completion ends. The symbols of *term* are constants; *c* is greatest in the precedence, so it never appears in a normal form.
- For an equation *lhs* == *rhs* as *term*, the result is [True]() when both sides have the same normal form. It is [False]() when completion ends with every critical pair joined and the sides have different normal forms: the system is then ground convergent. Otherwise it is the equation between the normal forms.
- When completion for *term* runs out of memory, [TKnuthBendixCompletion]() completes again with fewer steps, issues the message `TKnuthBendixCompletion::heap`, and gives the normal form under the smaller system.
- By default the ordering is the Knuth-Bendix ordering with weight 1 for every symbol. Its precedence puts the function symbols of the axioms first, by decreasing arity, then their constants, then the other symbols of *term*, so that the arguments of a commutative operator come out in canonical order.
- In an explicit precedence or weight list, the symbols left out follow the listed ones. A Knuth-Bendix weight is a nonnegative integer; weight 0 is allowed for a unary symbol that is greatest in the precedence.
- Completion drops critical pairs whose every ground instance already rewrites to a common term. Such ground joinability lets completion terminate on permutative equations such as commutativity.
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | 1 | seconds the completion runs |
| [MaxSteps]() | [Automatic]() | maximum number of critical pairs completion selects |
| [Method]() | `"Waldmeister"` | completion configuration, any [Method]() setting of [TFindProof]() |

- The derivations of the rules in the [ProofObject]() are built within the [TimeConstraint]() as well. When they take longer, the message `TKnuthBendixCompletion::nodata` is issued and the proof lists only the axioms.

## Basic Examples

Complete the group axioms under a precedence where the inverse is greatest:

```wl
TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, {OverBar, CircleTimes, 1}]
```

<!-- => ProofObject[...] -->

---

The ten rules of the completed system:

```wl
TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, {OverBar, CircleTimes, 1}]["Theorems"]
```

<!-- => {𝕒_ ⊗ 1 -> 𝕒, (𝕒_ ⊗ 𝕓_) ⊗ 𝕔_ -> 𝕒 ⊗ (𝕓 ⊗ 𝕔), 𝕒_ ⊗ OverBar[𝕒_] -> 1, 𝕒_ ⊗ (OverBar[𝕒_] ⊗ 𝕓_) -> 𝕓, 1 ⊗ 𝕒_ -> 𝕒, OverBar[1] -> 1, OverBar[OverBar[𝕒_]] -> 𝕒, OverBar[𝕒_] ⊗ 𝕒_ -> 1, OverBar[𝕒_] ⊗ (𝕒_ ⊗ 𝕓_) -> 𝕓, OverBar[𝕒_ ⊗ 𝕓_] -> OverBar[𝕓] ⊗ OverBar[𝕒]} -->

---

The normal form of a group expression:

```wl
TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, a \[CircleTimes] OverBar[b \[CircleTimes] (((c \[CircleTimes] OverBar[c]) \[CircleTimes] OverBar[d \[CircleTimes] b]) \[CircleTimes] a)], {OverBar, CircleTimes, 1}]
```

<!-- => d -->

## Scope

Name a theory of [AxiomaticTheory]():

```wl
TKnuthBendixCompletion["GroupAxioms", OverBar[OverBar[a]]]
```

<!-- => a -->

---

Commutativity cannot be oriented, so the completed system keeps it as an equation:

```wl
TKnuthBendixCompletion[{ForAll[{x, y}, x \[CirclePlus] y == y \[CirclePlus] x], ForAll[{x, y, z}, (x \[CirclePlus] y) \[CirclePlus] z == x \[CirclePlus] (y \[CirclePlus] z)]}]["Theorems"]
```

<!-- => {𝕒_ ⊕ 𝕓_ <-> 𝕓_ ⊕ 𝕒_, (𝕒_ ⊕ 𝕓_) ⊕ 𝕔_ -> 𝕒 ⊕ (𝕓 ⊕ 𝕔), 𝕒_ ⊕ (𝕓_ ⊕ 𝕔_) <-> 𝕓_ ⊕ (𝕒_ ⊕ 𝕔_), 𝕒_ ⊕ (𝕓_ ⊕ 𝕔_) <-> 𝕔_ ⊕ (𝕓_ ⊕ 𝕒_)} -->

---

Under that system, a sum of three constants written in any order has one normal form:

```wl
TKnuthBendixCompletion[{ForAll[{x, y}, x \[CirclePlus] y == y \[CirclePlus] x], ForAll[{x, y, z}, (x \[CirclePlus] y) \[CirclePlus] z == x \[CirclePlus] (y \[CirclePlus] z)]}, c \[CirclePlus] (b \[CirclePlus] a)]
```

<!-- => a ⊕ (b ⊕ c) -->

---

Refute an equation: the completed system has the rule `k[k[x]] -> x` and no other, and the two sides are distinct normal forms:

```wl
TKnuthBendixCompletion[ForAll[x, k[k[x]] == x], k[a] == a]
```

<!-- => False -->

---

Decide an equation in a group:

```wl
TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, OverBar[a \[CircleTimes] b] == OverBar[b] \[CircleTimes] OverBar[a], {OverBar, CircleTimes, 1}]
```

<!-- => True -->

---

Complete under the Knuth-Bendix ordering with weight 0 for the inverse:

```wl
Length[TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, {{OverBar, 0}, {CircleTimes, 1}, {1, 1}}]["Theorems"]]
```

<!-- => 10 -->

## Properties and Relations

The rules of a system without unorientable equations rewrite with [ReplaceRepeated]() to the same normal form:

```wl
po = TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, {OverBar, CircleTimes, 1}];
```

```wl
a \[CircleTimes] OverBar[b \[CircleTimes] (((c \[CircleTimes] OverBar[c]) \[CircleTimes] OverBar[d \[CircleTimes] b]) \[CircleTimes] a)] //. po["Theorems"]
```

<!-- => d -->

---

The rules are theorems of the axioms, and the proof object checks their derivations:

```wl
TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, {OverBar, CircleTimes, 1}]["ProofFunction"][]
```

<!-- => Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>] -->

## Possible Issues

With weight 1 for every symbol, the Knuth-Bendix ordering cannot orient the inverse of a product as `OverBar[a ⊗ b] -> OverBar[b] ⊗ OverBar[a]`, and the group axioms have no finite completed system. Completion then stops at [MaxSteps]() or the [TimeConstraint]() with a partial system:

```wl
Length[Quiet[TKnuthBendixCompletion[AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1, MaxSteps -> 100, TimeConstraint -> Infinity]]["Theorems"]]
```

<!-- => 30 -->
