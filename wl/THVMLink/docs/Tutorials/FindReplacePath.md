---
Template: TechNote
Name: FindReplacePath
Title: Finding Replacement Paths with Directed Rules
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/FindReplacePath
Keywords: [rewriting, directed rules, oriented rules, semi-Thue, string rewriting, replacement path, equational path, FindEquationalProof, FindReplacePath, multiway]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, Disproof]
---

[FindEquationalProof]() decides whether an equation follows from a set of equations. Its axioms work in both directions, and its proofs join the two sides of the goal: each side is rewritten until they meet at a common term. Many questions have a direction instead. Sorting a word, putting a product into right-nested form and expanding a product by the distributive law all apply rules from left to right only, and the object of interest is a replacement path, a chain of terms from a start to a target in which each term comes from the one before by one left-to-right application of a rule. It is the path that repeated use of [Replace]() and [ReplaceAt]() would trace.

THVMLink finds such paths with its equational prover. An axiom written as a rule rewrites from left to right only, and the path of a proof can be read back as a list of terms:

| | |
| --- | --- |
| <code>[TFindProof]()[*thm*, *axioms*]</code> | prove *thm*, with each rule *lhs* -> *rhs* among *axioms* applied from left to right only |
| <code>[TFindEquationalPath]()[*thm*, *axioms*]</code> | the path of terms from the left side of *thm* to its right side |
| <code>[TFindEquationalPath]()[*proof*, "*prop*"]</code> | a property, such as the rewrite at each step, of the path of a [ProofObject]() |
| <code>[TStringPath]()[*thm*, *axioms*]</code> | the path of words for a string rewriting theorem |
| <code>[TFindStringProof]()[*thm*, *axioms*]</code> | a [ProofObject]() for a string rewriting theorem |

[TFindEquationalPath]() is a port of the Wolfram Function Repository's <code>[ResourceFunction]()["FindEquationalPath"]</code>, which reads the same path off a two-way proof; with rule axioms, it gives a replacement path.

## A rule that rewrites in one direction

Associativity, read as a direction, moves parentheses to the right. Written with [Rule]() rather than [Equal](), it is an axiom the prover applies from left to right only. [CircleTimes]() is not [Orderless](), so the two sides of each equation stay distinct:

```wl
rassoc = {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]}
```
<!-- => {ForAll[{x, y, z}, (x ⊗ y) ⊗ z -> x ⊗ (y ⊗ z)]} -->

The goal is to bring a left-nested product of four factors into right-nested form:

```wl
goal = ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))]
```
<!-- => ForAll[{a, b, c, d}, ((a ⊗ b) ⊗ c) ⊗ d == a ⊗ (b ⊗ (c ⊗ d))] -->

[TFindProof]() proves it and gives the same kind of [ProofObject]() that [FindEquationalProof]() gives:

```wl
p = TFindProof[goal, rassoc, TimeConstraint -> 10]
```

Its proof function checks the proof independently of the search that found it:

```wl
p["ProofFunction"][]
```
<!-- => Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>] -->

## The replacement path

[TFindEquationalPath]() gives the chain of terms from the left side of the goal to its right side:

```wl
path = TFindEquationalPath[goal, rassoc, TimeConstraint -> 10]
```
<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

The outer parentheses move first, then the inner ones. Since the axiom is a rule and the right side of the goal is the fully rewritten form, every step is a forward application of the rule. [ReplaceList]() confirms it: each term of the path is among the results of applying the rule to the term before:

```wl
AllTrue[Partition[path, 2, 1], MemberQ[ReplaceList[#[[1]], (x_ \[CircleTimes] y_) \[CircleTimes] z_ :> x \[CircleTimes] (y \[CircleTimes] z)], #[[2]]] &]
```
<!-- => True -->

A [ProofObject]() already in hand gives the same path, read from its proof steps:

```wl
TFindEquationalPath[p]
```
<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

The proof graph shows those steps: the goal, taken as a hypothesis, is rewritten by the axiom into a lemma and then into the conclusion:

```wl
Graph[p["ProofGraph"], GraphLayout -> {"LayeredDigraphEmbedding", "Orientation" -> Left}]
```

## Each step of the path

The path has properties that describe each step. `"Rewrites"` gives, for each step, a [Replace]() or [ReplaceAt]() operator that performs it:

```wl
TFindEquationalPath[p, "Rewrites"]
```
<!-- => {Replace[((a ⊗ b) ⊗ c) ⊗ d :> (a ⊗ b) ⊗ (c ⊗ d)], Replace[(a ⊗ b) ⊗ (c ⊗ d) :> a ⊗ (b ⊗ (c ⊗ d))]} -->

Applying the operators in turn to the start of the path reaches its end:

```wl
Fold[#2[#1] &, First[path], TFindEquationalPath[p, "Rewrites"]]
```
<!-- => a ⊗ (b ⊗ (c ⊗ d)) -->

`"RewriteTest"` makes the same check for every intermediate term:

```wl
TFindEquationalPath[p, "RewriteTest"]
```
<!-- => Success["Rewrite", <|"MessageTemplate" -> "Running rewrite sequence reproduced the path."|>] -->

`"Substitutions"` gives the values of the variables of the rule at each step:

```wl
TFindEquationalPath[p, "Substitutions"]
```
<!-- => {<|x_ -> a ⊗ b, y_ -> c, z_ -> d|>, <|x_ -> a, y_ -> b, z_ -> c ⊗ d|>} -->

`"Justification"` gives the axiom or lemma used at each step, the direction it was used in and the position it was applied at. Here both steps use the first axiom in the direction it is written ([Right]()) at the top level (position `{}`):

```wl
TFindEquationalPath[p, "Justification"]
```
<!-- => {{{"Axiom", 1}, Right, {}}, {{"Axiom", 1}, Right, {}}} -->

A list of properties gives an [Association](), and [All]() gives every property, including the [ProofObject]() the path comes from.

## String rewriting

A semi-Thue system rewrites words: each rule replaces one subword by another wherever it occurs. [TStringPath]() and [TFindStringProof]() take words as strings and rules between them, and encode each word as a product of letters, adding the associativity axiom that lets a rule apply at any position. The single rule `"BA" -> "AB"` moves an `A` to the left past a `B`, so it sorts a word, and the path is the sorting:

```wl
TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, TimeConstraint -> 10]
```
<!-- => {"BBBAAA", "BBABAA", "BBAAAB", "BABAAB", "BAABAB", "BAAABB", "ABAABB", "AABABB", "AAABBB"} -->

Each step moves letters `A` to the left past a `B`. Some steps use a lemma that the prover derived from the axiom, the equation of `"BAA"` and `"AAB"`, so two letters `A` can pass a `B` in one step, as in the step from `"BBABAA"` to `"BBAAAB"`. The first and last words are the two sides of the theorem; the words between are the route the proof found, one of several possible routes.

[TFindStringProof]() gives the [ProofObject]() behind the path:

```wl
TFindStringProof["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, TimeConstraint -> 10]
```

It takes the specs of [TFindProof](), such as `"Status"`:

```wl
TFindStringProof["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, "Status", TimeConstraint -> 10]
```
<!-- => "Proved" -->

A list of theorems is proved as one conjunction, from one search, and gives one [ProofObject]():

```wl
TFindStringProof[{"BA" -> "AB", "BBA" -> "ABB"}, {"BA" -> "AB"}, TimeConstraint -> 10]
```

[TStringPath]() gives one path for each theorem of a list:

```wl
TStringPath[{"BBA" -> "ABB", "BAA" -> "AAB"}, {"BA" -> "AB"}, TimeConstraint -> 10]
```
<!-- => {{"BBA", "BAB", "ABB"}, {"BAA", "AAB"}} -->

## Direction belongs to the axioms

Equality is symmetric: a theorem `s == t` holds exactly when `t == s` does. So the reversed theorem, from the sorted word back to the unsorted one, is also proved from the rule `"BA" -> "AB"`. Its path is the sorting path read backwards, and each step undoes a rule application:

```wl
TStringPath["AAABBB" -> "BBBAAA", {"BA" -> "AB"}, TimeConstraint -> 10]
```
<!-- => {"AAABBB", "AABABB", "ABAABB", "BAAABB", "BAABAB", "BABAAB", "BBAAAB", "BBABAA", "BBBAAA"} -->

The same holds for terms. The reversed associativity goal is proved, and its path moves parentheses to the left:

```wl
TFindEquationalPath[a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)) == ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d, rassoc, TimeConstraint -> 10]
```
<!-- => {a ⊗ (b ⊗ (c ⊗ d)), (a ⊗ b) ⊗ (c ⊗ d), ((a ⊗ b) ⊗ c) ⊗ d} -->

What a rule axiom controls is the rewriting the prover does. A rule is applied from left to right only, so the rules derived from it form a terminating system in which every word has one sorted normal form, and a theorem stated from a start to a target that the rules reach gives a path of forward steps. An axiom written as an equation, with [TwoWayRule]() (`"BA" <-> "AB"`), a pair of words or [Equal](), can be applied in either direction, and the path can take a different route:

```wl
TStringPath["AAABBB" -> "BBBAAA", {"BA" <-> "AB"}, TimeConstraint -> 10]
```
<!-- => {"AAABBB", "AABABB", "ABAABB", "BAAABB", "BAABAB", "BABAAB", "BBAAAB", "BBAABA", "BBABAA", "BBBAAA"} -->

To get a replacement path, write the axioms as rules and state the theorem with the start term on the left and the target on the right. The general prover, with its methods and schedules, is described in [Theorem Proving with THVMLink](paclet:WolframInstitute/THVMLink/tutorial/ATP).
